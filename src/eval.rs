use bitboard;
use board;
use board::{Color, Rank, PieceType};
use position::Position;
use score;
use score::{PhaseScore, Score};

pub fn full(pos: &Position) -> Score {
    let mut ps = pos.psqt_score();
    ps += eval_pawns(pos);

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
const ISOLATION_BONUS: [[PhaseScore; 8]; 2] = [
    // Blocked
    [sc!(-6, -8), sc!(-6, -8), sc!(-6, -8), sc!(-8, -8), sc!(-8, -8), sc!(-6, -8), sc!(-6, -8), sc!(-6, -8)],
    // Open
    [sc!(-14, -16), sc!(-14, -17), sc!(-15, -18), sc!(-16, -20), sc!(-16, -20), sc!(-15, -18), sc!(-14, -17), sc!(-14, -16)],
];

fn eval_pawns(pos: &Position) -> PhaseScore {
    use bitboard::IntoBitboard;
    use board::File;
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

            let passed = bitboard::passer_mask(us, sq) & their_pawns == 0
                && bitboard::in_front_mask(us, sq) & our_pawns == 0;
            if passed {
                side_score[us.index()] += PASSER_BONUS[rel_rank.index()];
            }

            let open = bitboard::in_front_mask(us, sq) & their_pawns == 0;
            let neighbor_files = bitboard::neighbor_mask(sq.file());
            let isolated = neighbor_files & our_pawns == 0;
            if isolated {
                side_score[us.index()] += ISOLATION_BONUS[open as usize][sq.file().index()];
            }

            let connected = ((sq.pawn_push(them).rank().into_bitboard() & our_pawns) |
                             (sq.rank().into_bitboard() & our_pawns)) & neighbor_files != 0;
            if connected {
                side_score[us.index()] += sc!(5, 5);
            }

            // Only pawns that are behind a friendly pawn count as doubled.
            let doubled = bitboard::in_front_mask(us, sq) & our_pawns != 0;
            if doubled {
                side_score[us.index()] += sc!(-4, -8);
            }
        }
    }
    side_score[Color::White.index()] - side_score[Color::Black.index()]
}

#[cfg(test)]
mod tests {
    use board::Rank::*;
    use board::File::*;
    use position::Position;
    use ::eval::{PASSER_BONUS, ISOLATION_BONUS};

    chess_test!(test_eval_pawns, {
        let test_case = |fen, want| {
            let pos = Position::from_fen(fen);
            let got = ::eval::eval_pawns(&pos);
            assert_eq!(want, got);
        };
        // All pawns in starting positions.
        test_case("4k3/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - -", sc!(0, 0));
        // Empty board, white pawn on A2.
        test_case("4k3/8/8/8/8/8/P7/4K3 w - -", PASSER_BONUS[_2.index()] + ISOLATION_BONUS[1][A.index()]);
        // Empty board, black pawn on A2.
        test_case("4k3/8/8/8/8/8/p7/4K3 w - -", -(PASSER_BONUS[_7.index()] + ISOLATION_BONUS[1][A.index()]));
        // White pawn on A4, opposed black pawn on B5.
        test_case("4k3/8/8/1p6/P7/8/8/4K3 w - -",
                  ISOLATION_BONUS[1][A.index()] - ISOLATION_BONUS[1][B.index()]);
        // White pawn on A4, opposed black pawn on B4.
        test_case("4k3/8/8/8/Pp6/8/8/4K3 w - -", PASSER_BONUS[_4.index()] - PASSER_BONUS[_5.index()] +
                  ISOLATION_BONUS[1][A.index()] - ISOLATION_BONUS[1][B.index()]);
        // White pawns on A4 and B4, black pawn on B6.
        test_case("4k3/8/1p6/8/PP6/8/8/4K3 w - -",
                  sc!(5, 5) * 2 - ISOLATION_BONUS[0][B.index()]);
        // White pawns on A4 and B4, black pawn on C6.
        test_case("4k3/8/2p5/8/PP6/8/8/4K3 w - -",
                  sc!(5, 5) * 2 + PASSER_BONUS[_4.index()] - ISOLATION_BONUS[1][C.index()]);
        // White pawns on A4 and A5, black pawn on C6.
        test_case("4k3/8/2p5/P7/P7/8/8/4K3 w - -",
                  PASSER_BONUS[_5.index()] - sc!(4, 8) - PASSER_BONUS[_3.index()] +
                  ISOLATION_BONUS[1][A.index()] * 2 - ISOLATION_BONUS[1][C.index()]);
    });
}
