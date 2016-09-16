use std::sync::Mutex;

use bitboard;
use bitboard::{Bitboard};
use board;
use board::{Color, File, Piece, PieceType, Rank, Square};
use position;
use position::{HashKey, Position};
use score;
use score::{PhaseScore, Score};

pub fn full(pos: &Position) -> Score {
    let mut ps = pos.psqt_score();
    let mut ed = EvalData::new();
    ps += eval_pawns(pos, &mut ed);
    ps += eval_pieces(pos, &mut ed);

    let mut s = ps.interpolate(pos);
    if !can_win(pos.us(), pos) {
        s = min!(s, score::DRAW_SCORE);
    }
    if !can_win(pos.them(), pos) {
        s = max!(s, score::DRAW_SCORE);
    }
    s
}

fn can_win(side: Color, pos: &Position) -> bool {
    !(pos.pieces_of_color_and_type(side, PieceType::Pawn) == 0 &&
      pos.non_pawn_material(side) < score::mg_material(PieceType::Rook))
}

// Bonus for passed pawns, indexed by rank.
const PASSER_BONUS: [PhaseScore; 8] = [
    sc!(0, 0),
    sc!(5, 10), 
    sc!(10, 20),
    sc!(20, 25),
    sc!(60, 75),
    sc!(120, 135),
    sc!(200, 225),
    sc!(0, 0),
];

// Penalty for isolated pawns, indexed by whether or not there's an
// enemy pawn in front of us.
// TODO: try to condense this down so we don't have big tables.
const ISOLATION_BONUS: [[PhaseScore; 8]; 2] = [
    // Blocked
    [sc!(-6, -8), sc!(-6, -8), sc!(-6, -8), sc!(-8, -8), sc!(-8, -8), sc!(-6, -8), sc!(-6, -8), sc!(-6, -8)],
    // Open
    [sc!(-14, -16), sc!(-14, -17), sc!(-15, -18), sc!(-16, -20), sc!(-16, -20), sc!(-15, -18), sc!(-14, -17), sc!(-14, -16)],
];

const CANDIDATE_BONUS: [PhaseScore; 8] = [
    sc!(0, 0), sc!(5, 5), sc!(5, 10), sc!(10, 15), sc!(20, 30), sc!(30, 45), sc!(0, 0), sc!(0, 0)
];

struct EvalData {
    attacks_by: [[Bitboard; 8]; 2],
    open_files: u8,
    half_open_files: [u8; 2],
}

impl EvalData {
    pub fn new() -> EvalData {
        EvalData {
            attacks_by: [[0; 8]; 2],
            open_files: 0,
            half_open_files: [0, 0],
        }
    }
}

#[derive(Clone, Copy)]
struct PawnData {
    key: u32,
    score: [PhaseScore; 2], 
    passers: [Bitboard; 2],
    attacks: [Bitboard; 2],
    open_files: u8,
    half_open_files: [u8; 2],
}

impl PawnData {
    pub fn new() -> PawnData {
        PawnData {
            key: 0,
            score: [sc!(0, 0), sc!(0, 0)],
            passers: [0, 0],
            attacks: [0, 0],
            open_files: 0,
            half_open_files: [0, 0],
        }
    }
}

struct PawnCache {
    table: Vec<PawnData>,
}

impl<'a> PawnCache {
    pub fn new(bytes: usize) -> PawnCache {
        let mut buckets = 1;
        while (buckets << 1) * ::std::mem::size_of::<PawnData>() < bytes {
            buckets = buckets << 1;
        }
        PawnCache {
            table: vec![PawnData::new(); buckets],
        }
    }

    pub fn get(&'a mut self, key: HashKey) -> Option<&'a PawnData> {
        let idx = key as usize & (self.table.len() - 1);
        let short_key = (key >> 32) as u32;
        if short_key == self.table[idx].key {
            return Some(&mut self.table[idx]);
        }
        None
    }

    pub fn put(&mut self, pd: &PawnData, key: HashKey) {
        let idx = key as usize & (self.table.len() - 1);
        let entry: &mut PawnData = &mut self.table[idx];
        let short_key = (key >> 32) as u32;
        if short_key == entry.key {
            return;
        }
        *entry = pd.clone();
    }
}

lazy_static! {
    static ref GLOBAL_PAWN_CACHE: Mutex<PawnCache> = Mutex::new(PawnCache::new(1 << 20));
}

fn analyze_pawns(pos: &Position) -> PawnData {
    use bitboard::IntoBitboard;
    use board::PieceType::Pawn;

    if let Some(entry) = GLOBAL_PAWN_CACHE.lock().unwrap().get(pos.pawn_hash()) {
        return *entry;
    }

    let mut pd = PawnData::new();
    pd.key = (pos.pawn_hash() >> 32) as u32;

    let all_pawns = pos.pieces_of_type(Pawn);
    for us in board::each_color() {
        let them = us.flip();
        let our_pawns = pos.pieces_of_color_and_type(us, Pawn);
        let their_pawns = pos.pieces_of_color_and_type(them, Pawn);

        for f in 0..8 {
            let file_bb = bb!(File::from_u8(f));
            if our_pawns & file_bb == 0 {
                pd.half_open_files[us.index()] |= 1 << f;
            }
            if all_pawns & file_bb == 0 {
                pd.open_files |= 1 << f;
            }
        }

        let mut pawns_to_score = our_pawns;
        while pawns_to_score != 0 {
            let sq = bitboard::pop_square(&mut pawns_to_score);
            let rel_rank = sq.relative_to(us).rank();

            pd.attacks[us.index()] |= bitboard::pawn_attacks(us, sq);

            let our_passer_mask = bitboard::passer_mask(us, sq);
            let blockers = our_passer_mask & their_pawns;
            let in_front = bitboard::in_front_mask(us, sq);
            let neighbor_files = bitboard::neighbor_mask(sq.file());
            let passed = blockers == 0 && in_front & our_pawns == 0;
            if passed {
                // Passed pawn score is computed in eval_pawns.
                pd.passers[us.index()] |= bb!(sq);
            } else {
                // Candidate passed pawns (one enemy pawn one file away).
                if bitboard::in_front_mask(us, sq) & all_pawns == 0 && (blockers & neighbor_files).count_ones() < 2 {
                    pd.score[us.index()] += CANDIDATE_BONUS[rel_rank.index()];
                }
            }

            let open = bitboard::in_front_mask(us, sq) & their_pawns == 0;
            let isolated = neighbor_files & our_pawns == 0;
            if isolated {
                pd.score[us.index()] += ISOLATION_BONUS[open as usize][sq.file().index()];
            }

            // Only pawns that are behind a friendly pawn count as doubled.
            let doubled = bitboard::in_front_mask(us, sq) & our_pawns != 0;
            if doubled {
                pd.score[us.index()] -= sc!(6, 9);
            }

            let connected = ((sq.pawn_push(them).rank().into_bitboard() & our_pawns) |
                             (sq.rank().into_bitboard() & our_pawns)) & neighbor_files != 0;
            if connected {
                pd.score[us.index()] += sc!(5, 5);
            }

            use board::Square::*;
            if bb!(sq) & bb!(D4, E4, D5, E5) != 0 {
                pd.score[us.index()].mg += 4;
            } else if bb!(sq) & bb!(C4, C5, D3, E3, D6, E6, F4, F5) != 0 {
                pd.score[us.index()].mg += 2;
            }

            if !passed && !isolated && !connected &&
                bitboard::pawn_attacks(us, sq) & their_pawns == 0 &&
                bitboard::passer_mask(them, sq) & our_pawns == 0 &&
                rel_rank.index() < Rank::_6.index() {
                let mut adv = sq.pawn_push(us);
                if adv.rank().into_bitboard() & our_passer_mask & their_pawns != 0 {
                    pd.score[us.index()] -= sc!(6, 9);
                } else {
                    loop {
                        let adv2 = adv.pawn_push(us);
                        if adv2.rank().into_bitboard() & our_passer_mask & their_pawns != 0 {
                            pd.score[us.index()] -= sc!(6, 9);
                            break;
                        }
                        if adv.rank().into_bitboard() & our_passer_mask & our_pawns != 0 ||
                            adv2.relative_to(us).rank().index() >= Rank::_7.index() {
                            break;
                        }
                        adv = adv2;
                    }
                }
            }
        }
    }

    GLOBAL_PAWN_CACHE.lock().unwrap().put(&pd, pos.pawn_hash());
    pd
}

fn eval_pawns(pos: &Position, ed: &mut EvalData) -> PhaseScore {
    use bitboard::dist;
    let pd = analyze_pawns(pos);
    let mut side_score = pd.score;

    ed.open_files = pd.open_files;
    for us in board::each_color() {
        let them = us.flip();
        ed.attacks_by[us.index()][PieceType::Pawn.index()] = pd.attacks[us.index()];
        ed.attacks_by[us.index()][PieceType::AllPieces.index()] = pd.attacks[us.index()];
        ed.half_open_files[us.index()] = pd.half_open_files[us.index()];
        
        let mut passers_to_score = pd.passers[us.index()];
        while passers_to_score != 0 {
            let sq = bitboard::pop_square(&mut passers_to_score);
            let target = sq.pawn_push(us);
            let rel_rank = sq.relative_to(us).rank();
            let promote_sq = Square::new(sq.file(), Rank::_8.relative_to(us));

            let mut passer_score = sc!(0, 0);
            if pos.non_pawn_material(them) == 0 {
                // Other side is down to king and pawns. Can the king reach us?
                // This measure is conservative, which is fine.
                let prom_dist: i32 = (Rank::_8.index() - rel_rank.index() -
                    if us == pos.us() { 1 } else { 0 } -
                    if rel_rank.index() == Rank::_2.index() { 1 } else { 0 }) as i32;
                if prom_dist < dist(pos.king_sq(them), promote_sq) as i32 {
                    // Give partial credit for the queen based on how long
                    // it'll take us to convert.
                    passer_score.eg +=
                        (score::QUEEN.eg - score::PAWN.eg) * (5 - prom_dist) / 6;
                } else {
                    passer_score = PASSER_BONUS[rel_rank.index()];
                }
            } else {
                passer_score = PASSER_BONUS[rel_rank.index()];
            }

            // If the square in front of us is blocked, scale back the bonus.
            if pos.piece_at(target) != Piece::NoPiece {
                passer_score = passer_score * 3 / 4;
            }

            // Adjust endgame bonus based on king proximity.
            passer_score.eg += 5 * (dist(pos.king_sq(them), target) as i32 -
                                    dist(pos.king_sq(us), target) as i32) * rel_rank.index() as i32;
            
            side_score[us.index()] += passer_score;
        }
    }

    side_score[Color::White.index()] - side_score[Color::Black.index()]
}

// Mobility bonus tables, indexed by piece type and available squares.
// TODO: come up with some formula for these instead of a giant table.
const MOBILITY_BONUS: [[PhaseScore; 32]; 8] = [
    [sc!(0, 0); 32],

    // Pawn
    [sc!(0, 0), sc!(4, 12), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0)],

    // Knight
    [sc!(-8, -8), sc!(-4, -4), sc!(0, 0), sc!(4, 4), sc!(8, 8), sc!(12, 12), sc!(16, 16), sc!(18, 18),
     sc!(20, 20), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0)],

    // Bishop
    [sc!(-15, -15), sc!(-10, -10), sc!(-5, -5), sc!(0, 0), sc!(5, 5), sc!(10, 10), sc!(15, 15), sc!(20, 20),
     sc!(25, 25), sc!(30, 30), sc!(35, 35), sc!(40, 40), sc!(40, 40), sc!(40, 40), sc!(40, 40), sc!(40, 40),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0)],

    // Rook
    [sc!(-10, -10), sc!(-8, -6), sc!(-6, -2), sc!(-4, 2), sc!(-2, 6), sc!(0, 10), sc!(2, 14), sc!(4, 18),
     sc!(6, 22), sc!(8, 26), sc!(10, 30), sc!(12, 34), sc!(14, 38), sc!(16, 42), sc!(18, 46), sc!(20, 50),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0),
     sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0), sc!(0, 0)],

    // Queen
    [sc!(-20, -20), sc!(-19, -18), sc!(-18, -16), sc!(-17, -14), sc!(-16, -12), sc!(-15, -10), sc!(-14, -8), sc!(-13, -6),
     sc!(-12, -4), sc!(-11, -2), sc!(-10, 0), sc!(-9, 2), sc!(-8, 4), sc!(-7, 6), sc!(-6, 8), sc!(-5, 10),
     sc!(-4, 12), sc!(-3, 14), sc!(-2, 16), sc!(-1, 18), sc!(0, 20), sc!(1, 22), sc!(2, 24), sc!(3, 26),
     sc!(4, 28), sc!(5, 30), sc!(6, 32), sc!(7, 34), sc!(8, 36), sc!(9, 38), sc!(10, 40), sc!(11, 42)],

    [sc!(0, 0); 32],
    [sc!(0, 0); 32],
];

fn eval_pieces(pos: &Position, ed: &mut EvalData) -> PhaseScore {
    let mut side_score = [sc!(0, 0), sc!(0, 0)];
    let all_pieces = pos.all_pieces(); 
    
    for us in board::each_color() {
        let kattack = bitboard::king_attacks(pos.king_sq(us));
        ed.attacks_by[us.index()][PieceType::King.index()] |= kattack;
        ed.attacks_by[us.index()][PieceType::AllPieces.index()] |= kattack;

        let them = us.flip();
        let available_squares = !pos.pieces_of_color(us);

        // King safety counters. We only calculate king safety if there's
        // substantial material left on the board.
        let do_safety = pos.non_pawn_material(them) >= score::QUEEN.mg;
        let king_halo = if do_safety {
            bitboard::king_attacks(pos.king_sq(them))
        } else {
            0
        };
        let mut num_king_attackers = 0;
        let mut king_attack_weight = 0;

        for pt in board::each_piece_type() {
            if pt == PieceType::King { break }

            let mut pieces_to_score = pos.pieces_of_color_and_type(us, pt);
            while pieces_to_score != 0 {
                let sq = bitboard::pop_square(&mut pieces_to_score);
                let mob = match pt {
                    PieceType::Pawn => {
                        if pos.piece_at(sq.pawn_push(us)) == Piece::NoPiece { 1 } else { 0 }
                    },
                    PieceType::Knight => {
                        let attacks = bitboard::knight_attacks(sq);
                        ed.attacks_by[us.index()][PieceType::Knight.index()] |= attacks;
                        ed.attacks_by[us.index()][PieceType::AllPieces.index()] |= attacks;
                        if attacks & king_halo != 0 {
                            num_king_attackers += 1;
                            king_attack_weight += 16;
                        }
                        (attacks & available_squares).count_ones()
                    },
                    PieceType::Bishop => {
                        let attacks = bitboard::bishop_attacks(sq,
                                all_pieces ^ pos.pieces_of_color_and_type(us, PieceType::Queen));
                        ed.attacks_by[us.index()][PieceType::Bishop.index()] |= attacks;
                        ed.attacks_by[us.index()][PieceType::AllPieces.index()] |= attacks;
                        if attacks & king_halo != 0 {
                            num_king_attackers += 1;
                            king_attack_weight += 16;
                        }
                        (attacks & available_squares).count_ones()
                    },
                    PieceType::Rook => {
                        let attacks = bitboard::rook_attacks(sq,
                                all_pieces ^ (pos.pieces_of_color_and_type(us, PieceType::Queen) |
                                              pos.pieces_of_color_and_type(us, PieceType::Rook)));
                        ed.attacks_by[us.index()][PieceType::Rook.index()] |= attacks;
                        ed.attacks_by[us.index()][PieceType::AllPieces.index()] |= attacks;
                        if attacks & king_halo != 0 {
                            num_king_attackers += 1;
                            king_attack_weight += 32;
                        }
                        let f = 1 << sq.file().index();
                        if f & ed.half_open_files[us.index()] != 0 {
                            side_score[us.index()] += sc!(10, 5);
                            if f & ed.half_open_files[them.index()] != 0 {
                                side_score[us.index()] += sc!(10, 5);
                            }
                        }
                        // Bonus for being on the 7th rank if there are pawns on the 7th and the
                        // opposing king is on the 7th or 8th.
                        if sq.relative_to(us).rank() == Rank::_7 &&
                            pos.king_sq(them).relative_to(us).rank().index() >= Rank::_7.index() &&
                            pos.pieces_of_color_and_type(them, PieceType::Pawn) & bb!(sq.rank()) != 0 {
                            side_score[us.index()] += sc!(10, 20);
                        }
                        (attacks & available_squares).count_ones()
                    }
                    PieceType::Queen => {
                        let attacks = bitboard::queen_attacks(sq,
                                all_pieces ^ (pos.pieces_of_color_and_type(us, PieceType::Bishop) |
                                              pos.pieces_of_color_and_type(us, PieceType::Rook)));
                        ed.attacks_by[us.index()][PieceType::Queen.index()] |= attacks;
                        ed.attacks_by[us.index()][PieceType::AllPieces.index()] |= attacks;
                        if attacks & king_halo != 0 {
                            num_king_attackers += 1;
                            king_attack_weight += 64;
                        }
                        (attacks & available_squares).count_ones()
                    }
                    _ => 0,
                };
                side_score[us.index()] += MOBILITY_BONUS[pt.index()][mob as usize];
            }
        }

        if do_safety {
            let mut shield_value = king_shield_score(us, pos);
            if pos.castle_rights() != position::CASTLE_NONE {
                shield_value /= 8;
            }
            side_score[us.index()].mg += shield_value;
            if shield_value < 36 {
                king_attack_weight += 8;
                if shield_value < 18 {
                    king_attack_weight += 8;
                }
            }

            const KING_ATTACK_SCALE: [i32; 16] = [
                0, 0, 640, 800, 1120, 1200, 1280, 1280,
                1344, 1344, 1408, 1408, 1472, 1472, 1536, 1536];
            side_score[us.index()].mg += KING_ATTACK_SCALE[num_king_attackers] * king_attack_weight / 1024;

        }
    }

    for us in board::each_color() {
        let them = us.flip();
        // Targets are their pieces that are attacked but not defended.
        let targets = pos.pieces_of_color(them) &
            !ed.attacks_by[them.index()][PieceType::AllPieces.index()] &
            ed.attacks_by[us.index()][PieceType::AllPieces.index()];
        let num_targets = targets.count_ones() as i32;
        side_score[us.index()] += sc!(5, 5) * num_targets * num_targets;
    }

    side_score[Color::White.index()] - side_score[Color::Black.index()]
}

fn king_shield_score(c: Color, pos: &Position) -> Score {
    let mut score = king_shield_at(pos.king_sq(c), c, pos);
    if pos.can_castle_short(c) {
        let target = pos.possible_castles(c, 0).kdest;
        score = max!(score, king_shield_at(target, c, pos));
    }
    if pos.can_castle_long(c) {
        let target = pos.possible_castles(c, 1).kdest;
        score = max!(score, king_shield_at(target, c, pos));
    }
    score
}

fn king_shield_at(ksq: Square, c: Color, pos: &Position) -> Score {
    if ksq.relative_to(c).rank().index() >= Rank::_4.index() { return 0 }
    let big_shield = bitboard::king_attacks(ksq) | bitboard::king_attacks(ksq.pawn_push(c));
    let far_shield = big_shield ^ bitboard::king_attacks(ksq);
    let near_shield = bitboard::shift(far_shield, if c == Color::White { board::SOUTH } else { board::NORTH });
    let pawns = pos.pieces_of_color_and_type(c, PieceType::Pawn);
    4 * ((big_shield & pawns).count_ones() +
         (far_shield & pawns).count_ones() * 2 +
         (near_shield & pawns).count_ones() * 4) as Score
}

#[cfg(test)]
mod tests {
    //use board::Rank::*;
    //use board::File::*;
    //use position::Position;
    //use ::eval::{PASSER_BONUS, ISOLATION_BONUS};

    //chess_test!(test_eval_pawns, {
    //    let test_case = |fen, want| {
    //        let pos = Position::from_fen(fen);
    //        let got = ::eval::eval_pawns(&pos);
    //        assert_eq!(want, got);
    //    };
    //    // All pawns in starting positions.
    //    test_case("4k3/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - -", sc!(0, 0));
    //    // Empty board, white pawn on A2.
    //    test_case("4k3/8/8/8/8/8/P7/4K3 w - -", PASSER_BONUS[_2.index()] + ISOLATION_BONUS[1][A.index()]);
    //    // Empty board, black pawn on A2.
    //    test_case("4k3/8/8/8/8/8/p7/K7 w - -", -(PASSER_BONUS[_7.index()] + ISOLATION_BONUS[1][A.index()]));
    //    // White pawn on A4, opposed black pawn on B5.
    //    test_case("4k3/8/8/1p6/P7/8/8/4K3 w - -",
    //              ISOLATION_BONUS[1][A.index()] - ISOLATION_BONUS[1][B.index()]);
    //    // White pawn on A4, opposed black pawn on B4.
    //    test_case("3k4/8/8/8/Pp6/8/8/2K5 w - -",
    //              PASSER_BONUS[_4.index()] - PASSER_BONUS[_5.index()] +
    //              ISOLATION_BONUS[1][A.index()] - ISOLATION_BONUS[1][B.index()]);
    //    // White pawns on A4 and B4, black pawn on B6.
    //    test_case("4k3/8/1p6/8/PP6/8/8/4K3 w - -",
    //              sc!(5, 5) * 2 - ISOLATION_BONUS[0][B.index()]);
    //    // White pawns on A4 and B4, black pawn on C6.
    //    test_case("3k4/8/2p5/8/PP6/8/8/4K3 w - -",
    //              sc!(5, 5) * 2 + PASSER_BONUS[_4.index()] - ISOLATION_BONUS[1][C.index()]);
    //    // White pawns on A4 and A5, black pawn on C6.
    //    test_case("2k5/8/2p5/P7/P7/8/8/4K3 w - -",
    //              PASSER_BONUS[_5.index()] - PASSER_BONUS[_3.index()]  // passed pawns
    //              - sc!(6, 9)  // doubled pawns
    //              + ISOLATION_BONUS[1][A.index()] * 2 - ISOLATION_BONUS[1][C.index()]);  // isolated pawns
    //    // White pawns on D3, E4, black pawns on C5, E6, D6
    //    test_case("4k3/8/3pp3/2p5/4P3/3P4/8/4K3 w - -",
    //              sc!(-5, -5) * 2 // net two connected bonus for black
    //              - sc!(6, 9));   // one backward pawn for white
    //    // White pawns on D2, E4, black pawns on C5, E6, D6
    //    test_case("4k3/8/3pp3/2p5/4P3/8/3P4/4K3 w - -",
    //              -sc!(5, 5) * 3 // net three connected bonus for black
    //              -sc!(6, 9));   // one backward pawn for white
    //});
}
