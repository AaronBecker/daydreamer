use std::sync::{Arc, mpsc};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time;
use std::time::{Duration, Instant};

use board;
use movegen;
use movement::Move;
use options;
use position;
use position::{AttackData, Position};
use score;
use score::Score;

pub type Depth = f32;
pub const ONE_PLY: Depth = 1.;
pub const MAX_PLY: Depth = 128.;

pub const WAITING_STATE: usize = 0;
pub const SEARCHING_STATE: usize = 1;
pub const STOPPING_STATE: usize = 2;

#[derive(Clone)]
pub struct EngineState {
   pub state: Arc<AtomicUsize>,
}

impl EngineState {
   pub fn new() -> EngineState {
      EngineState {
         state: Arc::new(AtomicUsize::new(WAITING_STATE)),
      }
   }

   pub fn enter(&self, state: usize) {
      self.state.store(state, Ordering::Release);
   }

   pub fn load(&self) -> usize {
      self.state.load(Ordering::Acquire)
   }
}

pub struct SearchConstraints {
    pub infinite: bool,
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
            searchmoves: Vec::new(),
            node_limit: u64::max_value(),
            depth_limit: MAX_PLY,
            use_timer: false,
            hard_limit: Duration::new(0, 0),
            soft_limit: Duration::new(0, 0),
            start_time: time::Instant::now(),
        }
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
   m: Move,
   score: Score,
   pv: Vec<Move>,
}

impl RootMove {
   pub fn new(m: Move) -> RootMove {
      RootMove {
         m: m,
         score: score::NONE,
         pv: Vec::with_capacity(128),
      }
   }
}

pub struct SearchData {
    pub pos: Position,
    pub root_moves: Vec<RootMove>,
    pub constraints: SearchConstraints,
    pub stats: SearchStats,
    pub state: EngineState,
    pub uci_channel: mpsc::Receiver<String>,
}

impl SearchData {
    pub fn new() -> SearchData {
        // Dummy receiver channel to avoid uninitialized memory.
        let (_, rx) = mpsc::channel();
        SearchData {
            pos: Position::from_fen(position::START_FEN),
            root_moves: Vec::new(),
            constraints: SearchConstraints::new(),
            stats: SearchStats::new(),
            state: EngineState::new(),
            uci_channel: rx,
        }
    }

    pub fn should_stop(&self, depth: Depth, nodes: u64) -> bool {
        if self.constraints.infinite {
            return true;
        }
        if depth > self.constraints.depth_limit || nodes > self.constraints.node_limit {
            return true
        }
        if self.constraints.use_timer {
            return self.constraints.start_time.elapsed() > self.constraints.hard_limit
        }
        false
    }
}

pub fn go(data: &mut SearchData) {
   // We might have received a stop command already, in which case we
   // shouldn't start searching.
   if data.state.load() == STOPPING_STATE {
      return;
   }
   data.state.enter(SEARCHING_STATE);

   let ad = AttackData::new(&data.pos);
   let mut moves = data.constraints.searchmoves.clone();
   if moves.len() == 0 {
      movegen::gen_legal(&data.pos, &ad, &mut moves);
   }
   if moves.len() == 0 {
      println!("info string no moves to search");
      println!("bestmove (none)");
      return
   }
   for m in moves.iter() {
      data.root_moves.push(RootMove::new(*m));
   }

   deepening_search(data);

   println!("bestmove {}", data.root_moves[0].m);
   data.state.enter(WAITING_STATE);
}

fn should_deepen(data: &SearchData, d: Depth) -> bool {
   if data.state.load() == STOPPING_STATE { return false }
   if data.constraints.depth_limit <= d { return false }
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

fn deepening_search(data: &mut SearchData) {
   let mut depth = 0.;
   while should_deepen(data, depth) {
      if data.state.load() == STOPPING_STATE {
         break;
      }
      println!("searching");
      ::std::thread::sleep(time::Duration::from_secs(2));
      depth += ONE_PLY;
   }
}
