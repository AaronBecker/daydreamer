use position::{Position, AttackData, UndoState};
use movegen::gen_legal;

pub fn perft(pos: &mut Position, depth: u32) -> u64 {
    internal_perft(pos, depth, false)
}

pub fn divide(pos: &mut Position, depth: u32) -> u64 {
    internal_perft(pos, depth, true)
}

fn internal_perft(pos: &mut Position, depth: u32, divide: bool) -> u64 {
    if depth == 0 {
        return 1;
    }

    let ad = AttackData::new(pos);
    let moves = &mut Vec::with_capacity(128);
    gen_legal(pos, &ad, moves);
    if depth == 1 {
        return moves.len() as u64;
    }

    let mut count = 0;
    let undo = UndoState::undo_state(&pos);
    for mv in moves.iter() {
        pos.do_move(*mv, &ad);
        let subtree = perft(pos, depth - 1);
        pos.undo_move(*mv, &undo);
        if divide {
            println!("{:<5} {:15}", mv.to_string(), subtree);
        }
        count += subtree;
    }
    return count;

}

#[cfg(test)]
mod tests {
    use super::*;
    use position::Position;
    
    fn test_case(name: &str, fen: &str, depth: u32, expect: u64) {
        use bitboard;
        bitboard::initialize();
        let mut pos = Position::from_fen(fen);
        let count = perft(&mut pos, depth);
        assert!(count == expect, format!("{}, expected {} got {}", name, expect, count));
    }

    chess_test!(test_perft_fast, {
        test_case("en passant discovered check 1",
                  "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 4, 10138);
        test_case("en passant discovered check 2",
                  "8/8/8/8/k1p4R/8/3P4/3K4 w - - 0 1", 4, 10138);
        test_case("avoid illegal en passant capture 1",
                  "8/5bk1/8/2Pp4/8/1K6/8/8 w - d6 0 1", 4, 9287);
        test_case("avoid illegal en passant capture 2",
                  "8/8/1k6/8/2pP4/8/5BK1/8 b - d3 0 1", 4, 9287);
        test_case("en passant capture checks opponent 1",
                  "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 4, 13931);
        test_case("en passant capture checks opponent 2",
                  "8/5k2/8/2Pp4/2B5/1K6/8/8 w - d6 0 1", 4, 13931);
        test_case("short castling gives check 1",
                  "5k2/8/8/8/8/8/8/4K2R w K - 0 1", 4, 6399);
        test_case("short castling gives check 2",
                  "4k2r/8/8/8/8/8/8/5K2 b k - 0 1", 4, 6399);
        test_case("long castling gives check 1",
                  "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 4, 7418);
        test_case("long castling gives check 2",
                  "r3k3/8/8/8/8/8/8/3K4 b q - 0 1", 4, 7418);
        test_case("castling (including losing cr due to rook capture) 1",
                  "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 3, 27826);
        test_case("castling (including losing cr due to rook capture) 2",
                  "r3k2r/7b/8/8/8/8/1B4BQ/R3K2R b KQkq - 0 1", 3, 27826);
        test_case("castling prevented 1",
                  "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 3, 50509);
        test_case("castling prevented 2",
                  "r3k2r/8/5Q2/8/8/3q4/8/R3K2R w KQkq - 0 1", 3, 50509);
        test_case("promote out of check 1",
                  "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 4, 19174);
        test_case("promote out of check 1",
                  "3K4/8/8/8/8/8/4p3/2k2R2 b - - 0 1", 4, 19174);
        test_case("discovered check 1",
                  "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", 4, 31961);
        test_case("discovered check 2",
                  "5K2/8/1Q6/2N5/8/1p2k3/8/8 w - - 0 1", 4, 31961);
        test_case("promote to give check 1",
                  "4k3/1P6/8/8/8/8/K7/8 w - - 0 1", 4, 2661);
        test_case("promote to give check 2",
                  "8/k7/8/8/8/8/1p6/4K3 b - - 0 1", 4, 2661);
        test_case("underpromote to check 1",
                  "8/P1k5/K7/8/8/8/8/8 w - - 0 1", 4, 1329);
        test_case("underpromote to check 2",
                  "8/8/8/8/8/k7/p1K5/8 b - - 0 1", 4, 1329);
        test_case("self stalemate 1",
                  "K1k5/8/P7/8/8/8/8/8 w - - 0 1", 4, 63);
        test_case("self stalemate 2",
                  "8/8/8/8/8/p7/8/k1K5 b - - 0 1", 4, 63);
        test_case("stalemate/checkmate 1",
                  "8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 4, 926);
        test_case("stalemate/checkmate 2",
                  "8/8/8/8/1k6/8/K1p5/8 b - - 0 1", 4, 926);
        test_case("double check 1",
                  "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23527);
        test_case("double check 2",
                  "8/5k2/8/5N2/5Q2/2K5/8/8 w - - 0 1", 4, 23527);
        test_case("rook aliasing 1",
                  "1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1", 4, 85765);
        test_case("rook aliasing 2",
                  "4k2r/8/8/7r/8/8/1B6/1K6 w k - 0 1", 4, 85765);
        test_case("rook aliasing 3",
                  "1k6/8/8/8/R7/1n6/8/R3K3 b Q - 0 1", 4, 38751);
        test_case("rook aliasing 4",
                  "r3k3/8/1N6/r7/8/8/8/1K6 w q - 0 1", 4, 38751);
        test_case("kiwipete",
                  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862);
    });

    // deeper, more thorough perft tests are too expensive to run by default
    #[test] #[ignore]
    fn test_perft_slow() {
        test_case("en passant discovered check 1",
                  "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 6, 1134888);
        test_case("en passant discovered check 2",
                  "8/8/8/8/k1p4R/8/3P4/3K4 w - - 0 1", 6, 1134888);
        test_case("avoid illegal en passant capture 1",
                  "8/5bk1/8/2Pp4/8/1K6/8/8 w - d6 0 1", 6, 824064);
        test_case("avoid illegal en passant capture 2",
                  "8/8/1k6/8/2pP4/8/5BK1/8 b - d3 0 1", 6, 824064);
        test_case("en passant capture checks opponent 1",
                  "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 6, 1440467);
        test_case("en passant capture checks opponent 2",
                  "8/5k2/8/2Pp4/2B5/1K6/8/8 w - d6 0 1", 6, 1440467);
        test_case("short castling gives check 1",
                  "5k2/8/8/8/8/8/8/4K2R w K - 0 1", 6, 661072);
        test_case("short castling gives check 2",
                  "4k2r/8/8/8/8/8/8/5K2 b k - 0 1", 6, 661072);
        test_case("long castling gives check 1",
                  "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 6, 803711);
        test_case("long castling gives check 2",
                  "r3k3/8/8/8/8/8/8/3K4 b q - 0 1", 6, 803711);
        test_case("castling (including losing cr due to rook capture) 1",
                  "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 4, 1274206);
        test_case("castling (including losing cr due to rook capture) 2",
                  "r3k2r/7b/8/8/8/8/1B4BQ/R3K2R b KQkq - 0 1", 4, 1274206);
        test_case("castling prevented 1",
                  "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 4, 1720476);
        test_case("castling prevented 2",
                  "r3k2r/8/5Q2/8/8/3q4/8/R3K2R w KQkq - 0 1", 4, 1720476);
        test_case("promote out of check 1",
                  "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 6, 3821001);
        test_case("promote out of check 1",
                  "3K4/8/8/8/8/8/4p3/2k2R2 b - - 0 1", 6, 3821001);
        test_case("discovered check 1",
                  "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", 5, 1004658);
        test_case("discovered check 2",
                  "5K2/8/1Q6/2N5/8/1p2k3/8/8 w - - 0 1", 5, 1004658);
        test_case("promote to give check 1",
                  "4k3/1P6/8/8/8/8/K7/8 w - - 0 1", 6, 217342);
        test_case("promote to give check 2",
                  "8/k7/8/8/8/8/1p6/4K3 b - - 0 1", 6, 217342);
        test_case("underpromote to check 1",
                  "8/P1k5/K7/8/8/8/8/8 w - - 0 1", 6, 92683);
        test_case("underpromote to check 2",
                  "8/8/8/8/8/k7/p1K5/8 b - - 0 1", 6, 92683);
        test_case("self stalemate 1",
                  "K1k5/8/P7/8/8/8/8/8 w - - 0 1", 6, 2217);
        test_case("self stalemate 2",
                  "8/8/8/8/8/p7/8/k1K5 b - - 0 1", 6, 2217);
        test_case("stalemate/checkmate 1",
                  "8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 7, 567584);
        test_case("stalemate/checkmate 2",
                  "8/8/8/8/1k6/8/K1p5/8 b - - 0 1", 7, 567584);
        test_case("double check 1",
                  "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23527);
        test_case("double check 2",
                  "8/5k2/8/5N2/5Q2/2K5/8/8 w - - 0 1", 4, 23527);
        test_case("rook aliasing 1",
                  "1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1", 5, 1063513);
        test_case("rook aliasing 2",
                  "4k2r/8/8/7r/8/8/1B6/1K6 w k - 0 1", 5, 1063513);
        test_case("rook aliasing 3",
                  "1k6/8/8/8/R7/1n6/8/R3K3 b Q - 0 1", 5, 346695);
        test_case("rook aliasing 4",
                  "r3k3/8/1N6/r7/8/8/8/1K6 w q - 0 1", 5, 346695);
        test_case("kiwipete",
                  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603);
    }
}
