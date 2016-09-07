use bitboard;
use bitboard::Bitboard;
use board;
use board::*;
use movement::{Move, NO_MOVE};
use position;
use position::{AttackData, Position};
use score;
use score::Score;
use search;

#[derive(Debug, PartialEq, Eq)]
struct ScoredMove {
    m: Move,
    s: Score,
}

impl ScoredMove {
    pub fn new(m: Move) -> ScoredMove {
        ScoredMove {
            m: m,
            s: 0,
        }
    }
}

// Moves are categorized as loud or quiet; in non-check situations they're
// generated separately and loud moves have priority in search ordering.
//
// Loud moves are promotions to queen and all captures that aren't also
// underpromotions.
//
// Quiet moves are everything else: non-capturing moves that aren't
// promotion to queen, plus all underpromotions.
//
// Quiet checks are the subset of quiet moves that give check. The current
// implementation doesn't generate underpromotions that give check, but
// this is a matter of expediency rather than principle.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum GenerationType {
    Loud,
    Quiet,
    Both,
    QuietChecks,
}

fn add_moves(pos: &Position, from: Square, mut bb: Bitboard, moves: &mut Vec<ScoredMove>) {
    while bb != 0 {
        let to = bitboard::pop_square(&mut bb);
        moves.push(ScoredMove::new(Move::new(from, to, pos.piece_at(from), pos.piece_at(to))));
    }
}

fn add_slider_moves(pos: &Position,
                    mut source_bb: Bitboard,
                    mask: Bitboard,
                    moves: &mut Vec<ScoredMove>,
                    attack_fn: fn(Square, Bitboard) -> Bitboard) {
    while source_bb != 0 {
        let from = bitboard::pop_square(&mut source_bb);
        let bb = attack_fn(from, pos.all_pieces()) & mask;
        add_moves(pos, from, bb, moves);
    }
}

fn add_non_slider_moves(pos: &Position,
                        mut source_bb: Bitboard,
                        mask: Bitboard,
                        moves: &mut Vec<ScoredMove>,
                        attack_fn: fn(Square) -> Bitboard) {
    while source_bb != 0 {
        let from = bitboard::pop_square(&mut source_bb);
        let bb = attack_fn(from) & mask;
        add_moves(pos, from, bb, moves);
    }
}

fn add_piece_moves(pos: &Position, targets: Bitboard, moves: &mut Vec<ScoredMove>) {
    let our_pieces = pos.our_pieces();
    add_non_slider_moves(pos,
                         our_pieces & pos.pieces_of_type(PieceType::Knight),
                         targets,
                         moves,
                         bitboard::knight_attacks);
    add_non_slider_moves(pos,
                         our_pieces & pos.pieces_of_type(PieceType::King),
                         targets,
                         moves,
                         bitboard::king_attacks);
    add_slider_moves(pos,
                     our_pieces & pos.pieces_of_type(PieceType::Bishop),
                     targets,
                     moves,
                     bitboard::bishop_attacks);
    add_slider_moves(pos,
                     our_pieces & pos.pieces_of_type(PieceType::Rook),
                     targets,
                     moves,
                     bitboard::rook_attacks);
    add_slider_moves(pos,
                     our_pieces & pos.pieces_of_type(PieceType::Queen),
                     targets,
                     moves,
                     bitboard::queen_attacks);
}

fn add_piece_captures(pos: &Position, moves: &mut Vec<ScoredMove>) {
    add_piece_moves(pos, pos.their_pieces(), moves);
}

fn add_piece_non_captures(pos: &Position, moves: &mut Vec<ScoredMove>) {
    add_piece_moves(pos, !pos.all_pieces(), moves);
}

fn add_castles(pos: &Position, moves: &mut Vec<ScoredMove>) {
    let (us, them) = (pos.us(), pos.them());
    for side in 0..2 {
        if pos.castle_rights() & (position::WHITE_OO << (side * 2) << us.index()) == 0 {
            continue;
        }
        let ci = pos.possible_castles(us, side);
        let mut failed = pos.all_pieces() & ci.path != 0;
        let mut sq = ci.kdest;
        while !failed && sq != ci.king {
            if pos.attackers(sq) & pos.their_pieces() != 0 {
                failed = true;
            }
            sq = shift_sq(sq, ci.d);
        }
        if !failed && ci.may_discover_check {
            // In some Chess960 positions, castling can be illegal because it
            // would discover an attack from a piece that was masked by the
            // castling rook.
            if bitboard::rook_attacks(ci.kdest,
                                      pos.all_pieces() ^ bb!(ci.rook) &
                                      (pos.pieces_of_color_and_type(them, PieceType::Rook) |
                                       pos.pieces_of_color_and_type(them, PieceType::Queen))) != 0 {
                failed = true;
            }
        }
        if !failed {
            moves.push(ScoredMove::new(Move::new_castle(ci.king, ci.rook, pos.piece_at(ci.king))));
        }
    }
}

fn add_pawn_promotions(pos: &Position,
                       pawns: Bitboard,
                       mut mask: Bitboard,
                       d: Delta,
                       gt: GenerationType,
                       moves: &mut Vec<ScoredMove>) {
    mask &= bitboard::shift(pawns, d);
    while mask != 0 {
        let to = bitboard::pop_square(&mut mask);
        let from = board::shift_sq(to, -d);
        if gt != GenerationType::Quiet {
            moves.push(ScoredMove::new(Move::new_promotion(from, to, pos.piece_at(from), pos.piece_at(to), PieceType::Queen)));
        }
        if gt != GenerationType::Loud {
            moves.push(ScoredMove::new(Move::new_promotion(from, to, pos.piece_at(from), pos.piece_at(to), PieceType::Rook)));
            moves.push(ScoredMove::new(Move::new_promotion(from, to, pos.piece_at(from), pos.piece_at(to), PieceType::Bishop)));
            moves.push(ScoredMove::new(Move::new_promotion(from, to, pos.piece_at(from), pos.piece_at(to), PieceType::Knight)));
        }
    }
}

fn add_pawn_non_promotions(pos: &Position,
                           pawns: Bitboard,
                           mut mask: Bitboard,
                           d: Delta,
                           moves: &mut Vec<ScoredMove>) {
    mask &= bitboard::shift(pawns, d);
    while mask != 0 {
        let to = bitboard::pop_square(&mut mask);
        let from = board::shift_sq(to, -d);
        moves.push(ScoredMove::new(Move::new(from, to, pos.piece_at(from), pos.piece_at(to))));
    }
}

fn add_masked_pawn_moves(pos: &Position,
                         mask: Bitboard,
                         check_discoverers: Bitboard,
                         gt: GenerationType,
                         moves: &mut Vec<ScoredMove>) {
    let (us, them) = (pos.us(), pos.them());
    let up = if us == Color::White { NORTH } else { SOUTH };
    let pawns = pos.pieces_of_color_and_type(us, PieceType::Pawn);
    let empty = !pos.all_pieces();
    let mut push_once_mask = bitboard::shift(pawns, up) & empty & mask;
    let mut push_twice_mask = bitboard::shift(bitboard::shift(pawns, up) & empty, up) &
        empty & bitboard::relative_rank_bb(us, Rank::_4) & mask;
    let our_2 = bitboard::relative_rank_bb(us, Rank::_2);
    let our_7 = bitboard::relative_rank_bb(us, Rank::_7);

    if gt != GenerationType::Loud {
        add_pawn_non_promotions(pos, pawns & !our_7, push_once_mask, up, moves);
        add_pawn_non_promotions(pos, pawns & our_2, push_twice_mask, up + up, moves);
    }

    let promote_from_mask = pawns & our_7;
    let capture_from_mask = pawns & !promote_from_mask;
    add_pawn_promotions(pos, pawns & our_7, push_once_mask, up, gt, moves);

    if gt != GenerationType::Quiet || promote_from_mask != 0 {
        for d in [up + WEST, up + EAST].iter() {
            let their_pieces = pos.their_pieces();
            let promote_to_mask = their_pieces & bitboard::shift(promote_from_mask, *d) & mask;
            let capture_to_mask = their_pieces & bitboard::shift(capture_from_mask, *d) & mask;

            add_pawn_promotions(pos, promote_from_mask, promote_to_mask, *d, gt, moves);
            if gt != GenerationType::Quiet {
                add_pawn_non_promotions(pos, capture_from_mask, capture_to_mask, *d, moves);
            }
        }
    }

    if gt != GenerationType::Quiet && pos.ep_square() != Square::NoSquare {
        let mut bb = capture_from_mask & bitboard::pawn_attacks(them, pos.ep_square());
        while bb != 0 {
            moves.push(ScoredMove::new(
                    Move::new_en_passant(bitboard::pop_square(&mut bb),
                                         pos.ep_square(),
                                         Piece::new(us, PieceType::Pawn),
                                         Piece::new(them, PieceType::Pawn))));
        }
    }
}

fn add_pawn_moves(pos: &Position, moves: &mut Vec<ScoredMove>) {
    add_masked_pawn_moves(pos, !0, 0, GenerationType::Both, moves);
}

// gen_evasions appends all available pseudo-legal moves to moves. It is only
// valid when the side to move is in check, and it is the only move generation
// function that should be called under those circumstances.
fn gen_evasions(pos: &Position, moves: &mut Vec<ScoredMove>) {
    let us = pos.us();
    let ksq = pos.king_sq(us);
    let (mut slide_attack, mut num_checkers) = (0, 0);
    let mut checkers = pos.checkers();
    let mut check_sq = Square::NoSquare;

    debug_assert!(checkers != 0);
    while checkers != 0 {
       check_sq = bitboard::pop_square(&mut checkers);
       if pos.piece_at(check_sq).piece_type().index() >= PieceType::Bishop.index() {
           slide_attack |= bb!(check_sq) ^ bitboard::ray(check_sq, ksq);
       }
       num_checkers += 1;
    }

    // King escapes
    let bb = bitboard::king_attacks(ksq) & !slide_attack & !pos.our_pieces();
    add_moves(pos, ksq, bb, moves);
    if num_checkers > 1 {
        // More than one checker, non-king moves are impossible.
        return;
    }

    // Add moves that block or capture the checker.
    let mask = bitboard::between(ksq, check_sq) | bb!(check_sq); 
    let our_pieces = pos.our_pieces();
    add_non_slider_moves(pos,
                         our_pieces & pos.pieces_of_type(PieceType::Knight),
                         mask,
                         moves,
                         bitboard::knight_attacks);
    add_slider_moves(pos,
                     our_pieces & pos.pieces_of_type(PieceType::Bishop),
                     mask,
                     moves,
                     bitboard::bishop_attacks);
    add_slider_moves(pos,
                     our_pieces & pos.pieces_of_type(PieceType::Rook),
                     mask,
                     moves,
                     bitboard::rook_attacks);
    add_slider_moves(pos,
                     our_pieces & pos.pieces_of_type(PieceType::Queen),
                     mask,
                     moves,
                     bitboard::queen_attacks);
    add_masked_pawn_moves(pos, mask, 0, GenerationType::Both, moves);
}

fn gen_legal(pos: &Position, ad: &AttackData, moves: &mut Vec<ScoredMove>) {
    if pos.checkers() != 0 {
        gen_evasions(pos, moves);
    } else {
        add_castles(pos, moves);
        add_piece_captures(pos, moves);
        add_piece_non_captures(pos, moves);
        add_pawn_moves(pos, moves);
    }

    moves.retain(|m| {
        if ad.pinned == 0 && m.m.piece().piece_type() != PieceType::King && !m.m.is_en_passant() {
            true
        } else {
            pos.pseudo_move_is_legal(m.m, ad)
        }
    });
}

fn gen_loud(pos: &Position, moves: &mut Vec<ScoredMove>) {
    add_piece_captures(pos, moves);
    add_masked_pawn_moves(pos, !0, 0, GenerationType::Loud, moves);
}

fn gen_quiet(pos: &Position, moves: &mut Vec<ScoredMove>) {
    add_piece_non_captures(pos, moves);
    add_masked_pawn_moves(pos, !0, 0, GenerationType::Quiet, moves);
}

// Check a move *that was pseudo-legal in some position* to see if it's
// pseudo-legal in this position. We don't need to filter out nonsense
// moves (e.g. knight promotes to queen, pawn promotes on the third rank,
// bishop moves like a knight), but we do need to filter out moves that
// were pseudo-legal in other positions but aren't any more.
pub fn move_is_pseudo_legal(pos: &Position, m: Move, loose: bool) -> bool {
    // Check the contents of the source square and the side to move.
    if pos.piece_at(m.from()) != m.piece() || m.piece().color() != pos.us() {
        return false;
    }

    // Handle difficult special cases by actually generating the moves.
    if m.is_castle() || m.is_promote() || m.is_en_passant() {
        let mut pseudos = Vec::with_capacity(128);
        let sm = ScoredMove::new(m);
        if pos.checkers() != 0 {
            gen_evasions(pos, &mut pseudos);
            return pseudos.contains(&sm);
        }
        if m.is_castle() {
            add_castles(pos, &mut pseudos);
            return pseudos.contains(&sm);
        }
        if m.is_promote() || m.is_en_passant() {
            add_pawn_moves(pos, &mut pseudos);
            return pseudos.contains(&sm);
        }
        debug_assert!(false);
    }

    debug_assert!(m.promote() == PieceType::NoPieceType);

    // Check the destination square. This isn't correct for castles
    // and en-passant captures, so we can't check it up front.
    if pos.piece_at(m.to()) != m.capture() {
        return false;
    }

    if loose {
        return true;
    }

    // Make sure that sliding moves aren't occluded.
    let pt = m.piece().piece_type();
    if pt == PieceType::Bishop {
        if bitboard::bishop_attacks(m.from(), pos.all_pieces()) & bb!(m.to()) == 0 {
            return false
        }
    } else if pt == PieceType::Rook {
        if bitboard::rook_attacks(m.from(), pos.all_pieces()) & bb!(m.to()) == 0 {
            return false
        }
    } else if pt == PieceType::Queen {
        if bitboard::queen_attacks(m.from(), pos.all_pieces()) & bb!(m.to()) == 0 {
            return false
        }
    } else if pt == PieceType::Pawn {
        // Avoid blocked pawn double-pushes.
        if ((m.from().index() as i8) - (m.to().index() as i8)).abs() == 16 &&
            pos.piece_at(m.to().pawn_push(pos.them())) != Piece::NoPiece {
            return false;
        }
    }

    // If we're in check, we don't generate some moves that would
    // otherwise be considered pseudolegal.
    if pos.checkers() != 0 {
        if pt == PieceType::King {
            // We only generate legal king moves while in check.
            // The pseudo-legality checker depends on this property.
            let mut checkers = pos.checkers();
            while checkers != 0 {
                let check_sq = bitboard::pop_square(&mut checkers);
                if pos.piece_at(check_sq).piece_type().index() >= PieceType::Bishop.index() {
                    if bb!(m.to()) & (bb!(check_sq) ^ bitboard::ray(check_sq, m.from())) != 0 {
                        return false;
                    }
                }
            }
        } else {
            // We only generate king moves when in double check.
            let checkers = pos.checkers();
            if checkers.count_ones() > 1 { return false }

            let check_sq = bitboard::lsb(checkers);
            let mask = bitboard::between(pos.king_sq(pos.us()), check_sq) | bb!(check_sq); 
            if bb!(m.to()) & mask == 0 { return false }
        }
    }
    true
}


#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum SelectionPhase {
    Start,
    Root,
    Legal,
    TT,
    Loud,
    Quiescence,
    Killers,
    Quiet,
    BadCaptures,
    Evasions,
    Done,
}

const LEGAL_PHASES: &'static [SelectionPhase] = &[SelectionPhase::Start,
                                                  SelectionPhase::Legal,
                                                  SelectionPhase::Done];
const ROOT_PHASES: &'static [SelectionPhase] = &[SelectionPhase::Start,
                                                 SelectionPhase::Root,
                                                 SelectionPhase::Done];
const NORMAL_PHASES: &'static [SelectionPhase] = &[SelectionPhase::Start,
                                                   SelectionPhase::TT,
                                                   SelectionPhase::Loud,
                                                   SelectionPhase::Killers,
                                                   SelectionPhase::Quiet,
                                                   SelectionPhase::BadCaptures,
                                                   SelectionPhase::Done];
const EVASION_PHASES: &'static [SelectionPhase] = &[SelectionPhase::Start,
                                                    SelectionPhase::TT,
                                                    SelectionPhase::Evasions,
                                                    SelectionPhase::Done];
const QUIESCENCE_PHASES: &'static [SelectionPhase] = &[SelectionPhase::Start,
                                                       SelectionPhase::TT,
                                                       SelectionPhase::Quiescence,
                                                       SelectionPhase::Done];

pub struct MoveSelector {
    moves: Vec<ScoredMove>,
    bad_captures: Vec<ScoredMove>,
    phases: &'static [SelectionPhase],
    phase_index: usize,
    tt_move: Move,
    killers: [Move; 2],
    last_score: Score,
}

impl MoveSelector {
    pub fn new(pos: &Position,
               d: search::SearchDepth,
               node: &search::Node,
               tt_move: Move) -> MoveSelector {
        MoveSelector {
            moves: Vec::with_capacity(128),  // TODO: check the performance implications of a
                                             // smaller allocation.
                                             // Maybe also consider putting everything in a big
                                             // fixed buffer to avoid any allocations at all.
            bad_captures: Vec::with_capacity(32),
            phases: {
                if pos.checkers() != 0 {
                    EVASION_PHASES
                } else if search::is_quiescence_depth(d) {
                    QUIESCENCE_PHASES
                } else {
                    NORMAL_PHASES
                }
            },
            phase_index: 0,
            tt_move: tt_move,
            killers: node.killers,
            last_score: 0,
        }
    }

    pub fn legal() -> MoveSelector {
        MoveSelector {
            moves: Vec::with_capacity(128),
            bad_captures: Vec::new(),
            phases: LEGAL_PHASES,
            phase_index: 0,
            tt_move: NO_MOVE,
            killers: [NO_MOVE; 2],
            last_score: 0,
        }
    }

    pub fn root(sd: &search::SearchData) -> MoveSelector {
        let mut v = Vec::new();
        for rm in sd.root_moves.iter().rev() {
            v.push(ScoredMove{ m: rm.m, s: rm.score });
        }
        MoveSelector {
            moves: v,
            bad_captures: Vec::new(),
            phases: ROOT_PHASES,
            phase_index: 0,
            tt_move: NO_MOVE,
            killers: [NO_MOVE; 2],
            last_score: 0,
        }
    }

    fn gen(&mut self, pos: &Position, ad: &AttackData) {
        match self.phases[self.phase_index] {
            SelectionPhase::Start | SelectionPhase::Done => {
                panic!("move selection phase error");
            },
            SelectionPhase::TT => {
                if self.tt_move != NO_MOVE && move_is_pseudo_legal(pos, self.tt_move, false) {
                    self.moves.push(ScoredMove { m: self.tt_move, s: 1 });
                }
            },
            SelectionPhase::Root => {},
            SelectionPhase::Legal => gen_legal(pos, ad, &mut self.moves),
            SelectionPhase::Loud | SelectionPhase::Quiescence => gen_loud(pos, &mut self.moves),
            SelectionPhase::Killers => {
                if self.killers[1] != NO_MOVE && move_is_pseudo_legal(pos, self.killers[1], false) {
                    self.moves.push(ScoredMove { m: self.killers[1], s: search::MAX_HISTORY + 1 });
                }
                if self.killers[0] != NO_MOVE && move_is_pseudo_legal(pos, self.killers[0], false) {
                    self.moves.push(ScoredMove { m: self.killers[0], s: search::MAX_HISTORY + 2 });
                }
            },
            SelectionPhase::Quiet => gen_quiet(pos, &mut self.moves),
            SelectionPhase::BadCaptures => self.moves.append(&mut self.bad_captures),
            SelectionPhase::Evasions => gen_evasions(pos, &mut self.moves),
        }
    }

    // Once we have an SEE implementation we can tell the difference between
    // good and bad captures and can update a bunch of this code.
    fn order(&mut self, pos: &Position, history: &[Score; 64 * 16]) {
        // Note that we want the moves ordered worst to best, so we can
        // efficiently pop moves of the end of the vector.
        match self.phases[self.phase_index] {
            SelectionPhase::Start | SelectionPhase::Done => {
                panic!("move selection phase error");
            },
            SelectionPhase::TT => {
                return;
            },
            SelectionPhase::Root => { 
                let mut nth_move = 0;
                for m in self.moves.iter_mut().rev() {
                    nth_move += 1;
                    if m.s != score::MIN_SCORE {
                        m.s = score::MAX_SCORE - nth_move;
                        continue;
                    }
                    if m.m.is_capture() || m.m.is_promote() {
                        let see = pos.static_exchange_sign(m.m);
                        if see < 0 {
                            // SEE values less than zero are actually calculated
                            // out, so the value is meaningful.
                            m.s = search::MIN_HISTORY + see;
                        } else {
                            m.s = score::mg_material(m.m.capture().piece_type()) -
                                m.m.piece().piece_type().index() as Score + search::MAX_HISTORY;
                        }
                    } else {
                        m.s = history[search::SearchData::history_index(m.m)];
                    }
                }
            },
            SelectionPhase::Legal => { return },
            SelectionPhase::Loud | SelectionPhase::Quiescence => {
                // MVV/LVA
                for m in self.moves.iter_mut() {
                    m.s = score::mg_material(m.m.capture().piece_type()) -
                        m.m.piece().piece_type().index() as Score;
                }
            },
            SelectionPhase::Killers => {
                // Killers are ordered by gen.
                return 
            }
            SelectionPhase::Quiet => {
                for m in self.moves.iter_mut() {
                    // Push killers to the back since they've already been
                    // searched and will be skipped.
                    if m.m == self.killers[0] {
                        m.s = search::MIN_HISTORY - 1;
                    } else if m.m == self.killers[1] {
                        m.s = search::MIN_HISTORY;
                    } else {
                        m.s += history[search::SearchData::history_index(m.m)];
                    }
                }
            },
            SelectionPhase::BadCaptures => {
                // Scoring for bad captures already happended in the loud phase.
                // We just need to sort.
            },
            SelectionPhase::Evasions => {
                // Evasions don't get a bad capture phase, so do static exchange
                // evaluation now.
                for m in self.moves.iter_mut() {
                    if m.m.is_capture() || m.m.is_promote() {
                        let see = pos.static_exchange_sign(m.m);
                        if see < 0 {
                            // SEE values less than zero are actually calculated
                            // out, so the value is meaningful.
                            m.s = search::MIN_HISTORY + see;
                        } else {
                            m.s = score::mg_material(m.m.capture().piece_type()) -
                                m.m.piece().piece_type().index() as Score + search::MAX_HISTORY;
                        }
                    } else {
                        if m.m == self.killers[0] {
                            m.s = search::MAX_HISTORY + 2
                        } else if m.m == self.killers[1] {
                            m.s = search::MAX_HISTORY + 1
                        } else {
                            m.s = history[search::SearchData::history_index(m.m)];
                        }
                    }
                }
            },
        }

        // In principle this is less efficient than writing an insertion sort,
        // but in practice the difference seems to be small enough that it
        // doesn't matter.
        self.moves.sort_by_key(|x| x.s);
    }

    pub fn next(&mut self,
                pos: &Position,
                ad: &AttackData,
                history: &[Score; 64 * 16]) -> Option<Move> {
        loop {
            while self.moves.len() == 0 {
                self.phase_index += 1;
                if self.phases[self.phase_index] == SelectionPhase::Done {
                    return None
                }
                self.gen(pos, ad);
                self.order(pos, history);
            }

            let sm = self.moves.pop().unwrap();
            let phase = self.phases[self.phase_index];
            if phase != SelectionPhase::TT && sm.m == self.tt_move {
                continue
            }
            if phase == SelectionPhase::Loud {
                let see = pos.static_exchange_sign(sm.m);
                if see < 0 {
                    self.bad_captures.push(ScoredMove{ m: sm.m, s: see });
                    continue;
                }
                self.last_score = see;
            } else if phase == SelectionPhase::BadCaptures {
                self.last_score = -1;
            } else if phase == SelectionPhase::Quiet {
                if sm.m == self.killers[0] || sm.m == self.killers[1] {
                    continue;
                }
                self.last_score = sm.s;
            } else {
                self.last_score = sm.s;
            }
            return Some(sm.m)
        }
    }

    pub fn bad_move(&self) -> bool {
        self.last_score < 0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use movement::Move;
    use position::{AttackData, Position};
    use search;

    chess_test!(test_move_is_pseudo_legal, {
        let test_case = |fen, m, want| {
            let pos = Position::from_fen(fen);
            println!("{}", pos.debug_string());
            assert_eq!(want, move_is_pseudo_legal(&pos, m, false));
        };
        use board::Square::*;
        use board::Piece::*;
        test_case("8/2k5/8/8/8/8/2q1K3/8 w - -", Move::new(E2, E1, WK, NoPiece), true);
        test_case("8/2k5/8/8/8/8/2q2K2/8 w - -", Move::new(E2, D1, WK, NoPiece), false);
        test_case("8/2k5/8/8/8/8/2q2K2/8 w - -", Move::new(E2, F2, WK, NoPiece), false);
        test_case("6q1/2k5/8/8/8/8/8/4K2R w K -", Move::new_castle(E1, H1, WK), false);
        test_case("8/2k5/8/5P2/8/8/8/4K3 w - -", Move::new_en_passant(F5, E6, WP, BP), false);
        test_case("8/2k5/8/4pP2/8/8/8/4K3 w - e6", Move::new_en_passant(F5, E6, WP, BP), true);
        test_case("8/2k5/r7/1p3P2/8/3B4/8/4K3 w K -", Move::new(D3, F5, WB, BB), false);
        test_case("8/2k5/r7/1p3P2/8/3B4/8/4K3 w K -", Move::new(D3, A6, WB, BR), false);
        test_case("8/2k5/r7/1p3P2/8/3B4/8/4K3 w K -", Move::new(D3, B5, WB, BP), true);
        test_case("8/2k5/1B6/qp3P2/8/6b1/5R2/4K3 w K -", Move::new(B6, A5, WB, BQ), true);
        test_case("8/2k5/1B6/qp3P2/8/6b1/5R2/4K3 w K -", Move::new(F2, D2, WR, NoPiece), true);
        test_case("8/2k5/1B6/qp3P2/8/6b1/5R2/4K3 w K -", Move::new(E1, D1, WK, NoPiece), true);
        test_case("8/2k5/1B6/qp3P2/8/6b1/5R2/4K3 w K -", Move::new(E1, D2, WK, NoPiece), false);
        test_case("8/3k4/1B6/qp3P2/8/6b1/4R3/4K3 w K -", Move::new(B6, A5, WB, BQ), false);
        test_case("8/2k5/1B6/qp3P2/8/6b1/4R3/4K3 w K -", Move::new(E2, D2, WR, NoPiece), false);
        test_case("8/2k5/1B6/qp3P2/8/6b1/4R3/4K3 w K -", Move::new(E1, D1, WK, NoPiece), true);
        test_case("8/2k5/1B6/qp3P2/8/6b1/4R3/4K3 w K -", Move::new(E1, D2, WK, NoPiece), false);
        test_case("4k3/8/8/8/8/8/4P3/4K3 w - -", Move::new(E2, E4, WP, NoPiece), true);
        test_case("4k3/8/8/8/8/4p3/4P3/4K3 w - -", Move::new(E2, E4, WP, NoPiece), false);
        test_case("r1bqk2r/pppp1ppp/2nbpn2/3N4/4P3/3B1N2/PPPP1PPP/R1BQK2R b KQkq -",
                  Move::new(E6, D5, BP, WN), true);
        test_case("r2q1rk1/pb1p2pp/1pB1p3/2b2p2/2P5/4PN2/PPQ2PPP/R1B2RK1 b - - 0 4",
                  Move::new(G1, H1, WK, NoPiece), false);
    });

    chess_test!(test_legal_generation, {
        let test_case = |fen, expect_moves: &Vec<&str>| {
            let pos = Position::from_fen(fen);
            let ad = AttackData::new(&pos);
            let mut moves = Vec::new();
            for uci in expect_moves.iter() {
                moves.push(Move::from_uci(&pos, &ad, *uci));
            }
            moves.sort_by_key(|m| { m.as_u32() });

            let mut ms = MoveSelector::legal();
            let mut got = Vec::new();
            while let Some(m) = ms.next(&pos, &ad, &search::EMPTY_HISTORY) {
                got.push(m);
            }
            got.sort_by_key(|m| { m.as_u32() });
            assert_eq!(moves, got);
        };
        test_case("rb2k1Nr/7p/5P1b/1qp5/8/2P5/P2P3P/RN2K1nR w KQkq - 0 1",
                  &mut vec!("g8h6", "h1g1", "b1a3", "g8e7", "e1d1", "e1f2", "a2a3",
                            "d2d3", "h2h3", "c3c4", "f6f7", "a2a4", "d2d4", "h2h4"));
        test_case("rb2k1Nr/7p/5P1b/1qp5/8/2P5/P2P3P/RN2K1nR b KQkq - 0 1",
                  &mut vec!("h6d2", "b8h2", "a8a2", "h8g8", "b5b1", "g1e2", "g1f3", "g1h3",
                            "h6e3", "h6f4", "h6g5", "h6g7", "h6f8", "b8g3", "b8f4", "b8e5",
                            "b8d6", "b8a7", "b8c7", "a8a3", "a8a4", "a8a5", "a8a6", "a8a7",
                            "b5f1", "b5b2", "b5e2", "b5b3", "b5d3", "b5a4", "b5b4", "b5c4",
                            "b5a5", "b5a6", "b5b6", "b5c6", "b5b7", "b5d7", "e8d7", "e8f7",
                            "e8d8", "e8f8", "c5c4"));
        test_case("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1",
                  &mut vec!("e1g1", "e1c1", "a1b1", "a1c1", "a1d1", "a1a2", "a1a3", "a1a4",
                            "a1a5", "a1a6", "a1a7", "a1a8", "h1f1", "h1g1", "h1h2", "h1h3",
                            "h1h4", "h1h5", "h1h6", "h1h7", "h1h8", "e1d1", "e1f1", "e1d2",
                            "e1e2", "e1f2"));
        test_case("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1",
                  &mut vec!("e1g1", "e1c1", "a1b1", "a1c1", "a1d1", "a1a2", "a1a3", "a1a4",
                            "a1a5", "a1a6", "a1a7", "a1a8", "h1f1", "h1g1", "h1h2", "h1h3",
                            "h1h4", "h1h5", "h1h6", "h1h7", "h1h8", "e1d1", "e1f1", "e1d2",
                            "e1e2", "e1f2"));
        test_case("4k3/8/8/8/8/8/8/R3K2R w Kq - 0 1",
                  &mut vec!("e1g1", "a1b1", "a1c1", "a1d1", "a1a2", "a1a3", "a1a4", "a1a5",
                            "a1a6", "a1a7", "a1a8", "h1f1", "h1g1", "h1h2", "h1h3", "h1h4",
                            "h1h5", "h1h6", "h1h7", "h1h8", "e1d1", "e1f1", "e1d2", "e1e2",
                            "e1f2"));
        test_case("r3k2r/8/8/8/8/8/8/R3K2R b kq - 0 1",
                  &mut vec!("e8g8", "e8c8", "a8a1", "h8h1", "a8a2", "a8a3", "a8a4", "a8a5",
                            "a8a6", "a8a7", "a8b8", "a8c8", "a8d8", "h8h2", "h8h3", "h8h4",
                            "h8h5", "h8h6", "h8h7", "h8f8", "h8g8", "e8d7", "e8e7", "e8f7",
                            "e8d8", "e8f8"));
        test_case("r3k2r/8/8/2q5/8/8/8/R3K2R w KQkq - 0 1",
                  &mut vec!("a1a8", "h1h8", "a1b1", "a1c1", "a1d1", "a1a2", "a1a3", "a1a4",
                            "a1a5", "a1a6", "a1a7", "h1f1", "h1g1", "h1h2", "h1h3", "h1h4",
                            "h1h5", "h1h6", "h1h7", "e1d1", "e1f1", "e1d2", "e1e2"));
        test_case("4knn1/P5P1/3p4/PpP5/6n1/5b2/4PPP1/4K3 w - b6 0 1",
                  &mut vec!("e1d1", "e1f1", "e1d2", "e2e3", "g2g3", "a5a6", "c5c6", "e2e4",
                            "a7a8n", "a7a8b", "a7a8r", "a7a8q", "g7f8n", "g7f8b", "g7f8r",
                            "g7f8q", "g2f3", "e2f3", "c5d6", "a5b6", "c5b6"));
        test_case("4k3/p1pp1pb1/1n2pnp1/3PN3/Pp2P3/2N2Q1p/1PPBBPPP/R3K2R b KQ a3 0 1",
                  &mut vec!("b6a4", "b6d5", "f6e4", "f6d5", "b6c4", "b6a8", "b6c8", "f6g4",
                            "f6h5", "f6h7", "f6g8", "g7h6", "g7f8", "g7h8", "e8e7", "e8d8",
                            "e8f8", "b4b3", "g6g5", "a7a6", "c7c6", "d7d6", "a7a5", "c7c5",
                            "h3g2", "e6d5", "b4c3", "b4a3"));
        test_case("8/1k6/5N2/2q5/6n1/3P4/5K2/6Q1 w - - 0 1",
                  &mut vec!("f2e1", "f2f1", "f2e2", "f2g2", "f2f3", "f2g3"));
        test_case("r2q3r/p1ppkpb1/bn2pnp1/3PN3/NB2P3/5Q1p/PPP1BPPP/1R2K2R b K - 0 3",
                  &mut vec!("e7e8", "d7d6", "c7c5"));
        test_case("k4b2/6P1/8/1pP5/8/8/6P1/7K w - b6 0 1",
                  &mut vec!("h1g1", "h1h2", "g2g3", "c5c6", "g2g4", "g7g8n", "g7g8b",
                            "g7g8r", "g7g8q", "g7f8n", "g7f8b", "g7f8r", "g7f8q", "c5b6"));
        test_case("k4b2/6P1/8/1pP5/8/8/6P1/7K b - - 0 1",
                  &mut vec!("f8c5", "f8g7", "f8d6", "f8e7", "a8a7", "a8b7", "a8b8", "b5b4"));

    });

    chess_test!(test_quiet_checks, {
        let test_case = |fen, expect_moves: &Vec<&str>| {
            let pos = Position::from_fen(fen);
            let ad = AttackData::new(&pos);
            let mut moves = Vec::new();
            for uci in expect_moves.iter() {
                moves.push(Move::from_uci(&pos, &ad, *uci));
            }
            moves.sort_by_key(|m| { m.as_u32() });

            let mut sms = Vec::new();
            ::movegen::gen_quiet_checks(&pos, &ad, &mut sms);
            let mut got = Vec::new();
            for m in sms.iter() {
                got.push(m.m);
            }
            got.sort_by_key(|m| { m.as_u32() });
            assert_eq!(moves, got);
        };
        test_case("8/8/8/8/8/8/1R2P1k1/4K3 w - -", &mut vec!("e2e3", "e2e4"));
        test_case("8/8/8/8/8/8/1R2PPk1/4K3 w - -", &mut vec!());
        test_case("8/8/8/8/6k1/8/1R2PP2/4K3 w - -", &mut vec!("f2f3"));
    });
    // TODO: some tests for ordering, to be added after I implement,
    // at a minimum, bad capture deferment.
}
