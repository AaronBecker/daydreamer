use std::sync::{Arc, mpsc};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::thread;
use std::time;
use std::time::{Duration, Instant};

use bitboard;
use board;
use board::{Rank, PieceType};
use eval;
use movegen::MoveSelector;
use movement::{Move, NO_MOVE, NULL_MOVE};
use options;
use position;
use position::{AttackData, Position, UndoState};
use score;
use score::{Score, score_is_valid, is_mate_score};
use transposition;

const NULL_MOVE_ENABLED: bool = true;
const NULL_EVAL_MARGIN: Score = 200;

const TT_ENABLED: bool = true;
// All transposition entries generated in quiesce() are considered equally deep.
const QDEPTH: SearchDepth = 0.;

const RAZORING_ENABLED: bool = true;
const RAZOR_DEPTH: SearchDepth = 3.5;
const RAZOR_MARGIN: [Score; 4] = [0 /* unused */, 300, 300, 325];

const IID_ENABLED: bool = true;
const FUTILITY_ENABLED: bool = true;

// Inside the search, we keep the remaining depth to search as a floating point
// value to accomodate fractional extensions and reductions better. Elsewhere
// depths are all integers to accommodate depth-indexed arrays.
pub type SearchDepth = f32;
pub const ONE_PLY_F: SearchDepth = 1.;
pub const MAX_PLY_F: SearchDepth = 127.;

pub fn is_quiescence_depth(sd: SearchDepth) -> bool {
    sd < ONE_PLY_F
}

pub type Depth = usize;
pub const ONE_PLY: Depth = 1;
pub const MAX_PLY: Depth = 127;

// EngineState is an atomic value that tracks engine state. It's atomic so that
// we can safely signal the search to stop based on external inputs without
// requiring the search thread to poll input. Logically it should be an enum,
// but I don't know of a way to get atomic operations on an enum type.
#[derive(Clone)]
pub struct EngineState {
    pub state: Arc<AtomicUsize>,
    pub generation: Arc<AtomicUsize>,
}

pub const WAITING_STATE: usize = 0;
pub const SEARCHING_STATE: usize = 1;
pub const PONDERING_STATE: usize = 2;
pub const STOPPING_STATE: usize = 3;

impl EngineState {
    pub fn new() -> EngineState {
        EngineState {
            state: Arc::new(AtomicUsize::new(WAITING_STATE)),
            generation: Arc::new(AtomicUsize::new(0)),
        }
    }

    pub fn enter(&self, state: usize) {
        self.state.store(state, Ordering::SeqCst);
    }

    pub fn load(&self) -> usize {
        self.state.load(Ordering::SeqCst)
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum SearchResult {
     Aborted,
     FailHigh,
     FailLow,
     Exact,
}

// in_millis converts a duration to integer milliseconds. It's always at least
// 1, to avoid divide-by-zero errors.
pub fn in_millis(d: &Duration) -> u64 {
    1 + d.as_secs() * 1000 + d.subsec_nanos() as u64 / 1_000_000
}

// SearchConstraints track the conditions for a search as specified via UCI.
// This is mostly about how much searching we should do before stopping, but
// also includes a list of moves to consider at the root.
pub struct SearchConstraints {
    pub infinite: bool,
    pub ponder : bool,
    pub searchmoves: Vec<Move>, // TODO: this doesn't seem quite right here, maybe move out
    pub node_limit: u64,
    pub depth_limit: Depth,

    use_timer: bool,
    hard_limit: Duration,
    soft_limit: Duration,
    start_time: Instant,
}

impl SearchConstraints {
    pub fn new() -> SearchConstraints {
        SearchConstraints {
            infinite: false,
            ponder: false,
            searchmoves: Vec::new(),
            node_limit: u64::max_value(),
            depth_limit: MAX_PLY,
            use_timer: false,
            hard_limit: Duration::new(0, 0),
            soft_limit: Duration::new(0, 0),
            start_time: time::Instant::now(),
        }
    }

    pub fn clear(&mut self) {
        self.infinite = false;
        self.ponder = false;
        self.searchmoves.clear();
        self.node_limit = u64::max_value();
        self.depth_limit = MAX_PLY;
        self.use_timer = false;
        self.hard_limit = Duration::new(0, 0);
        self.soft_limit = Duration::new(0, 0);
        self.start_time = time::Instant::now();
    }

    pub fn set_timer(&mut self, us: board::Color, wtime: u32, btime: u32,
                     winc: u32, binc: u32, movetime: u32, movestogo: u32) {
        self.start_time = time::Instant::now();
        if wtime == 0 && btime == 0 && winc == 0 && binc == 0 && movetime == 0 {
            self.use_timer = false;
            return;
        }
        self.use_timer = true;
        
        if movetime != 0 {
            self.hard_limit = Duration::from_millis(max!(0, movetime - options::time_buffer()) as u64);
            self.soft_limit = self.hard_limit;
            return;
        }
        let time = if us == board::Color::White { wtime } else { btime };
        let inc = if us == board::Color::White { winc } else { binc };
        let (mut soft_limit, mut hard_limit);
        if movestogo != 0 {
            // x/y time control
            soft_limit = time / clamp!(movestogo, 2, 20);
            hard_limit = if movestogo == 1 {
                max!(time - 250, time / 2)
            } else {
                min!(time / 4, time * 4 / movestogo)
            };
        } else {
            // x+y time control
            soft_limit = time / 30 + inc;
            hard_limit = max!((time / 5) as i32, (inc as i32) - 250) as u32;
        }
        soft_limit = min!(soft_limit, time - options::time_buffer()) * 6 / 10;
        hard_limit = min!(hard_limit, time - options::time_buffer());
        self.soft_limit = Duration::from_millis(soft_limit as u64);
        self.hard_limit = Duration::from_millis(hard_limit as u64);
    }
}

pub struct SearchStats {
    nodes: u64,
    qnodes: u64,
    pvnodes: u64,
}

impl SearchStats {
    pub fn new() -> SearchStats {
        SearchStats {
            nodes: 0,
            qnodes: 0,
            pvnodes: 0,
        }
    }
}

pub struct RootMove {
    pub m: Move,
    pub score: Score,
    depth: Depth,
    pv: Vec<Move>,
}

impl RootMove {
    pub fn new(m: Move) -> RootMove {
        RootMove {
            m: m,
            score: score::MIN_SCORE,
            depth: 0,
            pv: Vec::with_capacity(MAX_PLY),
        }
    }
}


#[derive(Copy, Clone, Debug)]
pub struct Node {
    pub killers: [Move; 2],
}

impl Node {
    pub fn new() -> Node {
        Node {
            killers: [NO_MOVE, NO_MOVE],
        }
    }
}

pub struct SearchData {
    pub pos: Position,
    pub root_moves: Vec<RootMove>,
    pub current_depth: Depth,
    pub constraints: SearchConstraints,
    pub stats: SearchStats,
    pub state: EngineState,
    pub uci_channel: mpsc::Receiver<String>,
    pub pv_stack: [[Move; MAX_PLY + 1]; MAX_PLY + 1],
    pub search_stack: [Node; MAX_PLY + 1],
    pub history: [Score; 64 * 16],
    pub tt: transposition::Table,
}

pub const MAX_HISTORY: Score = 10000;
pub const MIN_HISTORY: Score = -10000;
pub const EMPTY_HISTORY: [Score; 64 * 16] = [0; 64 * 16];

impl SearchData {
    pub fn new() -> SearchData {
        // Dummy receiver channel to avoid uninitialized memory.
        let (_, rx) = mpsc::channel();
        SearchData {
            pos: Position::from_fen(position::START_FEN),
            root_moves: Vec::new(),
            current_depth: 0,
            constraints: SearchConstraints::new(),
            stats: SearchStats::new(),
            state: EngineState::new(),
            uci_channel: rx,
            pv_stack: [[NO_MOVE; MAX_PLY + 1]; MAX_PLY + 1],
            search_stack: [Node::new(); MAX_PLY + 1],
            history: [0; 64 * 16],
            tt: transposition::Table::new(16 << 20), // TODO: uci handling for table size
        }
    }

    pub fn reset(&mut self) {
        self.root_moves = Vec::new();
        self.current_depth = 0;
        self.stats = SearchStats::new();
        self.pv_stack = [[NO_MOVE; MAX_PLY + 1]; MAX_PLY + 1];
    }

    pub fn should_stop(&self) -> bool {
        let engine_state = self.state.load();
        if engine_state == PONDERING_STATE { return false }
        if engine_state == STOPPING_STATE { return true }
        if self.stats.nodes >= self.constraints.node_limit &&
           !self.constraints.infinite { return true }
        false
    }

    pub fn history_index(m: Move) -> usize {
        m.piece().index() << 6 | m.to().index()
    }

    pub fn record_success(&mut self, m: Move, d: SearchDepth) {
        let index = SearchData::history_index(m);
        self.history[index] += (d * d) as Score;
        if self.history[index] > MAX_HISTORY {
            for i in 0..(64 * 16) {
                self.history[i] = self.history[i] >> 1;
            }
        }
    }

    pub fn record_failure(&mut self, m: Move, d: SearchDepth) {
        let index = SearchData::history_index(m);
        self.history[index] -= (d * d) as Score;
        if self.history[index] < MIN_HISTORY {
            for i in 0..(64 * 16) {
                self.history[i] = self.history[i] >> 1;
            }
        }
    }

    pub fn init_ply(&mut self, ply: usize) {
        self.pv_stack[ply][ply] = NO_MOVE;
    }

    pub fn update_pv(&mut self, ply: usize, m: Move) {
        self.pv_stack[ply][ply] = m;
        let mut i = ply + 1;
        while self.pv_stack[ply + 1][i] != NO_MOVE {
            self.pv_stack[ply][i] = self.pv_stack[ply + 1][i];
            i += 1;
        }
        self.pv_stack[ply][i] = NO_MOVE;
    }
}

pub fn go(data: &mut SearchData) {
    // Spawn a thread that will wake up when we hit our time limit and change
    // our state to STOPPING if the search hasn't terminated yet. This lets
    // us avoid checking the timer in the search thread.
    {
        let current_gen = 1 + data.state.generation.fetch_add(1, Ordering::SeqCst);
        if data.constraints.use_timer && !data.constraints.infinite {
            let sleep_time = data.constraints.hard_limit;
            let engine_state = data.state.clone();
            thread::spawn(move || {
                thread::sleep(sleep_time);
                while engine_state.load() == PONDERING_STATE {
                    thread::sleep(sleep_time);
                }
                if engine_state.load() != SEARCHING_STATE ||
                    engine_state.generation.load(Ordering::SeqCst) != current_gen {
                    return;
                }
                engine_state.enter(STOPPING_STATE);
            });
        }
    }
    data.state.enter(if data.constraints.ponder { PONDERING_STATE } else { SEARCHING_STATE });
    data.reset();
 
    let ad = AttackData::new(&data.pos);
    if data.constraints.searchmoves.len() == 0 {
        let mut ms = MoveSelector::legal();
        while let Some(m) = ms.next(&data.pos, &ad, &data.history) {
            data.constraints.searchmoves.push(m);
        }
    }
    if data.constraints.searchmoves.len() == 0 {
        println!("info string no moves to search");
        println!("bestmove (none)");
        return
    }
    for m in data.constraints.searchmoves.iter() {
        data.root_moves.push(RootMove::new(*m));
    }
    data.tt.new_generation();
 
    deepening_search(data);

    loop {
        let engine_state = data.state.load();
        if engine_state == PONDERING_STATE ||
            data.constraints.infinite && engine_state == SEARCHING_STATE {
            thread::sleep(time::Duration::from_millis(1));
        } else {
            break
        }
    }
 
    // Note: we enter the waiting state before outputting to ensure that we
    // aren't still in a searching state when a followup command arrives.
    data.state.enter(WAITING_STATE);
    if data.constraints.use_timer {
        println!("info string time {} soft limit {} hard limit {}",
                 in_millis(&data.constraints.start_time.elapsed()),
                 in_millis(&data.constraints.soft_limit),
                 in_millis(&data.constraints.hard_limit));
    }
    print!("bestmove {}", data.root_moves[0].m);
    if data.root_moves[0].pv.len() > 0 {
        print!(" ponder {}", data.root_moves[0].pv[0]);
    }
    println!("");
}

fn should_deepen(data: &SearchData) -> bool {
    if data.current_depth == MAX_PLY - 1 { return false }
    if data.state.load() == PONDERING_STATE { return true }
    if data.should_stop() { return false }
    if data.constraints.infinite { return true }
    if data.constraints.depth_limit < data.current_depth { return false }
    if !data.constraints.use_timer { return true }
    // If we're much more than halfway through our time, we won't make it
    // through the first move of the next iteration anyway.
    if data.constraints.start_time.elapsed() > data.constraints.soft_limit {
        return false
    }
    true
}

fn should_print(data: &SearchData) -> bool {
    data.constraints.start_time.elapsed().as_secs() > 1
}

// print_pv_single prints the search data for a single root move.
fn print_pv_single(data: &SearchData, rm: &RootMove, ordinal: usize, alpha: Score, beta: Score) {
     let ms = in_millis(&data.constraints.start_time.elapsed());
     let nps = if ms < 20 {
         String::new()  // don't report nps if we just started.
     } else {
         format!("nps {} ", data.stats.nodes * 1000 / ms)
     };
     let mut pv = String::new();
     pv.push_str(&format!("{} ", rm.m));
     for m in rm.pv.iter() {
         pv.push_str(&format!("{} ", *m));
     }
     let bound = if rm.score <= alpha {
         String::from("upperbound ")
     } else if rm.score >= beta {
         String::from("lowerbound ")
     } else {
         String::new()
     };
     let score = if is_mate_score(rm.score) {
         let mut mate_in = (score::MATE_SCORE - rm.score.abs() + 1) / 2;
         if rm.score < 0 { mate_in *= -1; }
         format!("mate {}", mate_in)
     } else {
         format!("cp {}", rm.score)
     };
     println!("info multipv {} depth {} score {} {} alpha {} beta {} time {} nodes {} {}pv {}",
              ordinal, data.current_depth, score, bound, alpha, beta, ms, data.stats.nodes, nps, pv);
     debug_assert!(score_is_valid(rm.score));
}

// print_pv prints out the most up-to-date information about the current
// principal variations in the format expected by UCI.
fn print_pv(data: &SearchData, alpha: Score, beta: Score) {
    // We need to print the n highest-scoring moves. They may not be in order
    // so we extract them in order with a heap.
    use std::collections::BinaryHeap;

    let mut heap = BinaryHeap::new();
    for entry in data.root_moves.iter().enumerate() {
        let (i, rm) = entry;
        heap.push((rm.score, i));
    }

    for i in 0..options::multi_pv() {
        if let Some((_, idx)) = heap.pop() {
            print_pv_single(data, &data.root_moves[idx], i + 1, alpha, beta);
        } else {
            panic!("failed to extract root move for printing");
        }
    }
}

fn deepening_search(data: &mut SearchData) {
    data.current_depth = 1;
    let (mut alpha, mut beta, mut last_score) = (score::MIN_SCORE, score::MAX_SCORE, 0);
    while should_deepen(data) {
        if should_print(data) {
            println!("info depth {}", data.current_depth);
        }
        // Calculate aspiration search window.
        let mut consecutive_fail_highs = 0;
        let mut consecutive_fail_lows = 0;
        const ASPIRE_MARGIN: [Score; 5] = [10, 35, 75, 300, 600];
        if data.current_depth > 5 && options::multi_pv() == 1 {
            alpha = max!(last_score - ASPIRE_MARGIN[0], score::MIN_SCORE);
            beta = min!(last_score + ASPIRE_MARGIN[0], score::MAX_SCORE);
        }

        loop {
            let sd = data.current_depth as SearchDepth;
            last_score = search(data, 0, alpha, beta, sd);
            // TODO: try nodes searched under this move as a secondary key.
            data.root_moves.sort_by(|a, b| {
                if a.depth == b.depth {
                    b.score.cmp(&a.score)
                } else {
                    b.depth.cmp(&a.depth)
                }
            });
            if data.should_stop() { return }
            print_pv(data, alpha, beta);
            debug_assert!(score_is_valid(last_score));
            if last_score <= alpha {
                consecutive_fail_lows += 1;
                consecutive_fail_highs = 0;
            } else if last_score >= beta {
                consecutive_fail_lows = 0;
                consecutive_fail_highs += 1;
            } else {
                break;
            }
 
            // TODO: allow more time to resolve the search on multiple consecutive failures.
            if consecutive_fail_lows > 0 {
                if consecutive_fail_lows >= ASPIRE_MARGIN.len() {
                    alpha = score::MIN_SCORE;
                } else {
                    alpha = max!(last_score - ASPIRE_MARGIN[consecutive_fail_lows], score::MIN_SCORE);
                }
            }
            if consecutive_fail_highs > 0 {
                if consecutive_fail_highs >= ASPIRE_MARGIN.len() {
                    beta = score::MAX_SCORE;
                } else {
                    beta = min!(last_score + ASPIRE_MARGIN[consecutive_fail_highs], score::MAX_SCORE);
                }
            }
        }

        data.current_depth += ONE_PLY;
    }
}

fn search(data: &mut SearchData, ply: usize,
          mut alpha: Score, mut beta: Score, depth: SearchDepth) -> Score {
    data.init_ply(ply);
    if data.should_stop() { return score::DRAW_SCORE; }
    if is_quiescence_depth(depth) {
        return quiesce(data, ply, alpha, beta, depth);
    }

    let root_node = ply == 0;
    if !root_node {
        alpha = max!(alpha, score::mated_in(ply));
        beta = min!(beta, score::mate_in(ply + 1));
        if alpha >= beta { return alpha }
        if data.pos.is_draw() { return score::DRAW_SCORE }
    }

    let orig_alpha = alpha;
    let open_window = beta - alpha > 1;

    let (mut tt_move, mut tt_score) = (NO_MOVE, score::MIN_SCORE);
    if root_node {
        tt_move = data.root_moves[0].m;
    } else {
        if let Some(entry) = data.tt.get(data.pos.hash()) {
            //println!("{:ply$}tt hit: m={}, depth={}, score={}", ' ', entry.m, entry.depth, entry.score, ply = ply);
            tt_move = entry.m;
            if depth as u8 <= entry.depth {
                if (entry.score >= beta as i16 && entry.score_type & score::AT_LEAST != 0) ||
                    (entry.score <= alpha as i16 && entry.score_type & score::AT_MOST != 0) {
                    tt_score = entry.score as Score;
                }
            }
        } else {
            //println!("{:ply$}tt miss", ' ', ply = ply);
        }
        if !open_window && tt_score != score::MIN_SCORE {
            debug_assert!(score_is_valid(tt_score));
            return tt_score;
        }
    }

    let lazy_score = data.pos.psqt_score().interpolate(&data.pos);
    let margin = beta - lazy_score;
    let depth_index = depth as usize;

    if FUTILITY_ENABLED &&
        !root_node &&
        depth <= 5. &&
        data.pos.checkers() == 0 &&
        data.pos.non_pawn_material(data.pos.us()) != 0 &&
        (tt_move == NO_MOVE || tt_score > score::mated_in(MAX_PLY)) &&
        lazy_score - (2 * (85. + 15. * depth + 2. * depth * depth) as Score) > beta {
            return lazy_score - (2 * (85. + 15. * depth + 2. * depth * depth) as Score)
    }

    if NULL_MOVE_ENABLED &&
        !open_window &&
        depth > 1. &&
        data.pos.last_move() != NULL_MOVE &&
        lazy_score + NULL_EVAL_MARGIN > beta &&
        !is_mate_score(beta) &&
        data.pos.checkers() == 0 &&
        data.pos.non_pawn_material(data.pos.us()) != 0 {
        // Nullmove search.
        let undo = UndoState::undo_state(&data.pos);
        data.pos.do_nullmove();
        let null_r = 2.0 + ((depth + 2.0) / 4.0) +
            clamp!(0.0, 1.5, (lazy_score-beta) as SearchDepth / 100.0);
        let null_score = -search(data, ply + 1, -beta, -beta + 1, depth - null_r);
        data.pos.undo_nullmove(&undo);
        if null_score >= beta { return beta }
    } else if RAZORING_ENABLED &&
        !open_window &&
        data.pos.last_move() != NULL_MOVE &&
        depth <= RAZOR_DEPTH &&
        tt_move == NO_MOVE &&
        data.pos.checkers() == 0 &&
        !is_mate_score(beta) &&
        lazy_score + RAZOR_MARGIN[depth_index] < beta {
        if depth <= 1.0 {
            return quiesce(data, ply, alpha, beta, 0.);
        }
        let qbeta = beta - RAZOR_MARGIN[depth_index];
        let qscore = quiesce(data, ply, qbeta - 1, qbeta, 0.);
        if qscore < qbeta {
            debug_assert!(score_is_valid(qscore));
            return max!(alpha, qscore);
        }
    }

    if IID_ENABLED && tt_move == NO_MOVE &&
        ((open_window && depth >= 5. && margin <= 300) ||
         (!open_window && depth >= 8. && margin <= 150)) {
        let iid_depth = if open_window {
            (4. * depth / 5.) - 2.
        } else {
            (2. * depth / 3.) - 2.
        };
        search(data, ply, alpha, beta, iid_depth);
        if let Some(entry) = data.tt.get(data.pos.hash()) {
            tt_move = entry.m;
        } else {
            tt_move = NO_MOVE;
        }
    }

    let (mut best_score, mut best_move) = (score::MIN_SCORE, NO_MOVE);
    let ad = AttackData::new(&data.pos);
    let undo = UndoState::undo_state(&data.pos);
    let mut num_moves = 0;

    let mut selector = if root_node {
        MoveSelector::root(&data)
    } else {
        MoveSelector::new(&data.pos, depth, &data.search_stack[ply], tt_move)
    };
    let (mut searched_quiets, mut searched_quiet_count) = ([NO_MOVE; 128], 0);
    while let Some(m) = selector.next(&data.pos, &ad, &data.history) {
        let mut root_idx = 0;
        if root_node {
            if !data.constraints.searchmoves.contains(&m) {
                continue
            }
            if should_print(data) {
                println!("info currmove {} currmovenumber {}", m, num_moves + 1);
            }
            root_idx = data.root_moves.iter().position(|x| x.m == m).unwrap();
        }

        // gives_check is not precise, but it's just used for heuristic extensions.
        let gives_check = !m.is_castle() && !m.is_en_passant() &&
            ((ad.potential_checks[m.piece().piece_type().index()] & bitboard::bb(m.to()) != 0) ||
             (ad.check_discoverers & bitboard::bb(m.from()) != 0 &&
              bitboard::ray(m.from(), m.to()) & bitboard::bb(ad.their_king) == 0));
        let deep_pawn = m.piece().piece_type() == PieceType::Pawn &&
            m.to().relative_to(data.pos.us()).rank().index() == Rank::_7.index();
        let quiet_move = !m.is_capture() && !m.is_promote();

        let ext = if gives_check && data.pos.static_exchange_sign(m) >=0 {
            1.
        } else {
            0.
        };

        if FUTILITY_ENABLED &&
            !root_node &&
            ext == 0. &&
            !deep_pawn &&
            depth <= 5. &&
            data.pos.checkers() == 0 &&
            num_moves >= depth_index + 2 &&
            m.promote() != PieceType::Queen &&
            best_score > score::mated_in(MAX_PLY) {
            // Value pruning.
            if lazy_score + score::mg_material(m.capture().piece_type()) +
                ((85. + 15. * depth + 2. * depth * depth) as Score) <
                beta + 2 * num_moves as Score {
                continue
            }

            // History pruning.
            // TODO: clean up the history interface; this is kind of ugly.
            if quiet_move && depth <= 4. && data.history[SearchData::history_index(m)] < 0 {
                continue
            }
        }

        if !data.pos.pseudo_move_is_legal(m, &ad) { continue }
        data.pos.do_move(m, &ad);
        //if ply < 5 { println!("{:ply$}ply {}, do_move {}", ' ', ply, m, ply = ply); }
        data.stats.nodes += 1;
        num_moves += 1;
        let mut score = score::MIN_SCORE;
        let mut full_search = (open_window && num_moves == 1) ||
                              (root_node && num_moves <= options::multi_pv());
        if !full_search {
            let mut lmr_red = 0.;
            if searched_quiet_count > 0 {
                lmr_red = 1.;
                if selector.bad_move() {
                    lmr_red += 1.;
                    if num_moves > 6 {
                        lmr_red += 0.5;
                    }
                    if searched_quiet_count > 6 {
                        lmr_red += 0.5;
                    }
                }
            }
            if lmr_red > 0. {
                score = -search(data, ply + 1, -alpha - 1, -alpha, depth + ext - lmr_red - ONE_PLY_F);
                debug_assert!(score_is_valid(score));
            } else {
                score = alpha + 1;
            }
            if score > alpha {
                score = -search(data, ply + 1, -alpha - 1, -alpha, depth + ext - ONE_PLY_F);
                debug_assert!(score_is_valid(score));
                if open_window && score > alpha { full_search = true; }
            }
        }
        if full_search {
            score = -search(data, ply + 1, -beta, -alpha, depth + ext - ONE_PLY_F);
            debug_assert!(score_is_valid(score));
        }
        debug_assert!(score_is_valid(score));
        data.pos.undo_move(m, &undo);
        if quiet_move && searched_quiet_count < 127 {
            searched_quiets[searched_quiet_count] = m;
            searched_quiet_count += 1;
        }
        // If we're aborting, the score from the last move shouldn't be trusted,
        // since we didn't finish searching it, so bail out without updating
        // pv, bounds, etc.
        if data.should_stop() { return score::DRAW_SCORE; }
        //if ply < 5 { println!("{:ply$}ply {}, un_move {} score = {}", ' ', ply, m, score, ply = ply); }

        if root_node {
            data.root_moves[root_idx].score = score::MIN_SCORE;
            data.root_moves[root_idx].depth = depth as Depth;
            if full_search || score > alpha {
                // We have updated move info for the root.
                data.root_moves[root_idx].score = score;
                data.root_moves[root_idx].pv.clear();
                for ply in 1..MAX_PLY {
                    let mv = data.pv_stack[1][ply];
                    if mv == NO_MOVE { break }
                    data.root_moves[root_idx].pv.push(mv);
                }
            }
            if score > alpha && score < beta && num_moves > options::multi_pv() {
                print_pv(data, alpha, beta)
            }
            debug_assert!(score_is_valid(data.root_moves[root_idx].score) || num_moves > 0);
        }

        if score > best_score {
            best_score = score;
            best_move = m;
            if score > alpha {
                alpha = score;
                if open_window { data.update_pv(ply, m) }
            }
            if score >= beta {
                if !m.is_capture() && !m.is_promote() {
                    if data.search_stack[ply].killers[0] != m {
                        data.search_stack[ply].killers[1] = data.search_stack[ply].killers[0];
                        data.search_stack[ply].killers[0] = m;
                    }
                    data.record_success(m, depth);
                    for i in 0..searched_quiet_count-1 {
                        data.record_failure(searched_quiets[i], depth);
                    }
                }
                debug_assert!(score_is_valid(score));
                data.tt.put(data.pos.hash(), m, depth, score, score::AT_LEAST);
                return beta;
            }
        }
    }
    if num_moves == 0 {
        // Stalemate or checkmate. We have to be careful not to prune away any
        // moves without checking their legality until we know that there's at
        // least one legal move so that this check is valid.
        best_score = if data.pos.checkers() != 0 {
            score::mated_in(ply)
        } else {
            score::DRAW_SCORE
        };
    }
    debug_assert!(score_is_valid(best_score));
    data.tt.put(data.pos.hash(), best_move, depth, best_score,
                if best_score <= orig_alpha { score::AT_MOST } else { score::EXACT });
    best_score
}

fn quiesce(data: &mut SearchData, ply: usize,
           mut alpha: Score, mut beta: Score, depth: SearchDepth) -> Score {
    alpha = max!(alpha, score::mated_in(ply));
    beta = min!(beta, score::mate_in(ply + 1));
    if alpha >= beta { return alpha }
    if data.pos.is_draw() { return score::DRAW_SCORE }
    if ply >= MAX_PLY { return score::DRAW_SCORE }
    data.init_ply(ply);
    let open_window = beta - alpha > 1;
    let orig_alpha = alpha;

    let (mut tt_hit, mut tt_depth) = (false, 0);
    let (mut tt_move, mut tt_score, mut tt_score_type) = (NO_MOVE, score::MIN_SCORE, 0);
    if TT_ENABLED {
        if let Some(entry) = data.tt.get(data.pos.hash()) {
            //println!("{:ply$}tt hit: m={}, depth={}, score={}", ' ', entry.m, entry.depth, entry.score, ply = ply);
            tt_hit = true;
            tt_move = entry.m;
            tt_score = entry.score as Score;
            tt_score_type = entry.score_type;
            tt_depth = entry.depth;
            debug_assert!(score_is_valid(tt_score));
        } else {
            //println!("{:ply$}tt miss", ' ', ply = ply);
        }
        if !open_window &&
            tt_hit &&
            depth as i8 <= tt_depth as i8 &&
            ((tt_score >= beta && tt_score_type & score::AT_LEAST != 0) ||
             (tt_score <= alpha && tt_score_type & score::AT_MOST != 0)) {
            return tt_score;
        }
    }

    let (mut best_move, mut best_score) = (NO_MOVE, score::MIN_SCORE);
    let mut static_eval = eval::full(&data.pos);
    debug_assert!(score_is_valid(static_eval));
    if data.pos.checkers() == 0 {
        best_score = static_eval;
        if best_score >= alpha {
            alpha = best_score;
            if tt_score != score::MIN_SCORE &&
                ((tt_score > best_score && tt_score_type & score::AT_LEAST != 0) ||
                    (tt_score < best_score && tt_score_type & score::AT_MOST != 0)) {
                best_score = tt_score;
                if !score::is_mate_score(tt_score) {
                    static_eval = tt_score;
                }
            }
            if best_score >= beta {
                debug_assert!(score_is_valid(best_score));
                return beta;
            }
        }
    }

    let ad = AttackData::new(&data.pos);
    let undo = UndoState::undo_state(&data.pos);
    let mut num_moves = 0;

    let allow_futility = data.pos.checkers() == 0;

    let mut selector = MoveSelector::new(&data.pos, depth, &data.search_stack[ply], tt_move);
    while let Some(m) = selector.next(&data.pos, &ad, &data.history) {
        if allow_futility &&
            m.promote() != PieceType::Queen &&
            static_eval + score::mg_material(m.capture().piece_type()) + 65 < alpha {
            continue
        }

        if data.pos.checkers() == 0 && data.pos.static_exchange_sign(m) < 0 { continue }
        if !data.pos.pseudo_move_is_legal(m, &ad) { continue }
        data.pos.do_move(m, &ad);
        //println!("{:ply$}ply {} qsearch, do_move {}", ' ', ply, m, ply = ply);
        data.stats.nodes += 1;
        num_moves += 1;

        let score = -quiesce(data, ply + 1, -beta, -alpha, depth - ONE_PLY_F);
        debug_assert!(score_is_valid(score));
        data.pos.undo_move(m, &undo);
        //println!("{:ply$}ply {} qsearch, un_move {} score {}", ' ', ply, m, score, ply = ply);

        // If we're aborting, the score from the last move shouldn't be trusted,
        // since we didn't finish searching it, so bail out without updating
        // pv, bounds, etc.
        if data.should_stop() { return score::DRAW_SCORE; }
        if score > best_score {
            best_score = score;
            best_move = m;
            if score > alpha {
                alpha = score;
                if open_window { data.update_pv(ply, m) }
            }
            if score >= beta {
                debug_assert!(score_is_valid(score));
                data.tt.put(data.pos.hash(), m, QDEPTH, score, score::AT_LEAST);
                return beta;
            }
        }
    }
    // Detect checkmate. We can't find stalemate because we don't reliably generate
    // quiet moves, but evasion movegen is always exhaustive.
    if num_moves == 0 && data.pos.checkers() != 0 {
        best_score = score::mated_in(ply);
    }
    debug_assert!(score_is_valid(best_score));
    data.tt.put(data.pos.hash(), best_move, QDEPTH, best_score,
                if best_score <= orig_alpha { score::AT_MOST } else { score::EXACT });
    best_score
}
