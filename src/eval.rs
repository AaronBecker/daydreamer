use board::{Color, PieceType};
use position::Position;
use score;
use score::Score;

pub fn full(pos: &Position) -> Score {
    let ps = pos.psqt_score();

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

// Penalty for isolated pawns, indexed by file and by whether or not there an
// enemy pawn in front of us.
const ISOLATION_BONUS: [[PhaseScore; 8]; 2] = [
    // Blocked
    [sc!(-6, -8), sc!(-6, -8), sc!(-6, -8), sc!(-8, -8), sc!(-8, -8), sc!(-6, -8), sc!(-6, -8), sc!(-6, -8)],
    // Open
    [sc!(-14, -16), sc!(-14, -17), sc!(-15, -18), sc!(-16, -20), sc!(-16, -20), sc!(-15, -18), sc!(-14, -17), sc!(-14, -16)],
];

fn eval_pawns(pos: &Position) -> PhaseScore {
    use board::PieceType::Pawn;
    // TODO: for now we're calculating from scratch, but this can be made
    // much more efficient with a pawn cache.
    let mut side_score = [sc!(0, 0), sc!(0, 0)];
    for us in board::each_color() {
        let them = us.flip();
        let our_pawns = pos.pieces_of_color_and_type(us, Pawn);
        let their_pawns = pos.pieces_of_color_and_type(them, Pawn);

        let mut pawns_to_score = our_pawns;
        while pawns_to_score != 0 {
            let sq = bitboard::pop_square(&mut pawns_to_score);
            let rel_rank = sq.relative_to(us).rank();

            let passed = bitboard::passer_mask(us, sq) & their_pawns == 0;
            let open = bitboard::in_front_mask(us, sq) & their_pawns == 0;
            let isolated = bitboard::neighbor_mask(sq.file()) & our_pawns == 0;

            if passed {
                side_score[us.index()] += PASSER_BONUS[rel_rank.index()];
            }
            if isolated {
                side_score[us.index()] += ISOLATION_BONUS[open as usize][sq.file().index()]
            }
        }
    }
    side_score[Color::White.index()] - side_score[Color::Black.index()]
}

//#[cfg(test)]
//mod tests {
//    use super::*;
//    use board::*;
//    use board::Square::*;
//
//    chess_test!(test_passed_pawns, {
//    });
//}
>>>>>>> Stashed changes
