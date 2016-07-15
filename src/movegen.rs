use board;
use board::*;
use position;
use position::Position;
use movement::Move;
use bitboard;
use bitboard::Bitboard;

fn add_moves(pos: &Position, from: Square, mut bb: Bitboard, moves: &mut Vec<Move>) {
    while bb != 0 {
        let to = bitboard::pop_square(&mut bb);
        moves.push(Move::new(from, to, pos.piece_at(from), pos.piece_at(to)));
    }
}

fn add_slider_moves(pos: &Position,
                    mut source_bb: Bitboard,
                    mask: Bitboard,
                    moves: &mut Vec<Move>,
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
                        moves: &mut Vec<Move>,
                        attack_fn: fn(Square) -> Bitboard) {
    while source_bb != 0 {
        let from = bitboard::pop_square(&mut source_bb);
        let bb = attack_fn(from) & mask;
        add_moves(pos, from, bb, moves);
    }
}

fn add_piece_moves(pos: &Position, targets: Bitboard, moves: &mut Vec<Move>) {
    let our_pieces = pos.pieces_of_color(pos.us());
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

pub fn add_piece_captures(pos: &Position, moves: &mut Vec<Move>) {
    add_piece_moves(pos, pos.pieces_of_color(pos.them()), moves);
}

pub fn add_piece_non_captures(pos: &Position, moves: &mut Vec<Move>) {
    add_piece_moves(pos, !pos.all_pieces(), moves);
}

pub fn add_castles(pos: &Position, moves: &mut Vec<Move>) {
    let (us, them) = (pos.us(), pos.them());
    for side in 0..2 {
        if pos.castle_rights() & (position::WHITE_OO << (side * 2) << us.index()) == 0 {
            continue;
        }
        let ci = pos.possible_castles(us, side);
        let mut failed = pos.all_pieces() & ci.path != 0;
        let mut sq = ci.kdest;
        while !failed && sq != ci.king {
            if pos.attackers(sq) & pos.pieces_of_color(them) != 0 {
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
            moves.push(Move::new_castle(ci.king, ci.rook, pos.piece_at(ci.king)));
        }
    }
}

pub fn add_pawn_promotions(pos: &Position,
                           pawns: Bitboard,
                           mut mask: Bitboard,
                           d: Delta,
                           moves: &mut Vec<Move>) {
    mask &= bitboard::shift(pawns, d);
    while mask != 0 {
        let to = bitboard::pop_square(&mut mask);
        let from = board::shift_sq(to, -d);
        moves.push(Move::new_promotion(from, to, pos.piece_at(from), pos.piece_at(to), PieceType::Knight));
        moves.push(Move::new_promotion(from, to, pos.piece_at(from), pos.piece_at(to), PieceType::Bishop));
        moves.push(Move::new_promotion(from, to, pos.piece_at(from), pos.piece_at(to), PieceType::Rook));
        moves.push(Move::new_promotion(from, to, pos.piece_at(from), pos.piece_at(to), PieceType::Queen));
    }
}

fn add_pawn_non_promotions(pos: &Position,
                           pawns: Bitboard,
                           mut mask: Bitboard,
                           d: Delta,
                           moves: &mut Vec<Move>) {
    mask &= bitboard::shift(pawns, d);
    while mask != 0 {
        let to = bitboard::pop_square(&mut mask);
        let from = board::shift_sq(to, -d);
        moves.push(Move::new(from, to, pos.piece_at(from), pos.piece_at(to)));
    }
}

fn add_masked_pawn_moves(pos: &Position,
                         mask: Bitboard,
                         moves: &mut Vec<Move>) {
    let (us, them) = (pos.us(), pos.them());
    let up = if us == Color::White { NORTH } else { SOUTH };
    let pawns = pos.pieces_of_color_and_type(us, PieceType::Pawn);
    let empty = !pos.all_pieces();
    let push_once_mask = bitboard::shift(pawns, up) & empty & mask;
    let push_twice_mask = bitboard::shift(bitboard::shift(pawns, up) & empty, up) &
        empty & bitboard::relative_rank_bb(us, Rank::_4) & mask;
    let our_2 = bitboard::relative_rank_bb(us, Rank::_2);
    let our_7 = bitboard::relative_rank_bb(us, Rank::_7);

    add_pawn_non_promotions(pos, pawns & !our_7, push_once_mask, up, moves);
    add_pawn_non_promotions(pos, pawns & our_2, push_twice_mask, up + up, moves);
    add_pawn_promotions(pos, pawns & our_7, push_once_mask, up, moves);

    let promote_from_mask = pawns & our_7;
    let capture_from_mask = pawns & !promote_from_mask;
    for d in [up + WEST, up + EAST].iter() {
        let promote_to_mask = pos.pieces_of_color(them) & bitboard::shift(promote_from_mask, *d) & mask;
        let capture_to_mask = pos.pieces_of_color(them) & bitboard::shift(capture_from_mask, *d) & mask;

        add_pawn_promotions(pos, promote_from_mask, promote_to_mask, *d, moves);
        add_pawn_non_promotions(pos, capture_from_mask, capture_to_mask, *d, moves);
    }

    if pos.ep_square() != Square::NoSquare {
        let mut bb = capture_from_mask & bitboard::pawn_attacks(them, pos.ep_square());
        while bb != 0 {
            moves.push(Move::new_en_passant(bitboard::pop_square(&mut bb),
                                            pos.ep_square(),
                                            Piece::new(us, PieceType::Pawn),
                                            Piece::new(them, PieceType::Pawn)));
        }
    }
}

pub fn add_pawn_moves(pos: &Position, moves: &mut Vec<Move>) {
    add_masked_pawn_moves(pos, !0, moves);
}

#[cfg(test)]
mod tests {
    use super::*;
    use board::*;
    use board::Square::*;
    use bitboard;
    use movement::*;
    use position::Position;

    chess_test!(test_add_piece_moves, {
        let test_case = |fen, expect_captures: &mut Vec<Move>, expect_non_captures: &mut Vec<Move>| {
            let pos = Position::from_fen(fen);
            expect_captures.sort_by_key(|m| { m.as_u32() });
            expect_non_captures.sort_by_key(|m| { m.as_u32() });
            let mut moves = Vec::new();
            add_piece_captures(&pos, &mut moves);
            moves.sort_by_key(|m| { m.as_u32() });
            assert_eq!(*expect_captures, moves);

            moves.clear();
            add_piece_non_captures(&pos, &mut moves);
            moves.sort_by_key(|m| { m.as_u32() });
            assert_eq!(*expect_non_captures, moves);
        };
        test_case("rb2k1Nr/7p/5P1b/1qp5/8/2P5/P2P3P/RN2K1nR w KQkq - 0 1",
                  &mut vec!(Move::new(H1, G1, Piece::WR, Piece::BN),
                            Move::new(G8, H6, Piece::WN, Piece::BB)),
                  &mut vec!(Move::new(B1, A3, Piece::WN, Piece::NoPiece),
                            Move::new(G8, E7, Piece::WN, Piece::NoPiece),
                            Move::new(E1, D1, Piece::WK, Piece::NoPiece),
                            Move::new(E1, E2, Piece::WK, Piece::NoPiece),
                            Move::new(E1, F2, Piece::WK, Piece::NoPiece),
                            Move::new(E1, F1, Piece::WK, Piece::NoPiece)));

        test_case("rb2k1Nr/7p/5P1b/1qp5/8/2P5/P2P3P/RN2K1nR b KQkq - 0 1",
                  &mut vec!(Move::new(A8, A2, Piece::BR, Piece::WP),
                            Move::new(B5, B1, Piece::BQ, Piece::WN),
                            Move::new(B8, H2, Piece::BB, Piece::WP),
                            Move::new(H8, G8, Piece::BR, Piece::WN),
                            Move::new(H6, D2, Piece::BB, Piece::WP)),
                  &mut vec!(Move::new(A8, A7, Piece::BR, Piece::NoPiece),
                            Move::new(A8, A6, Piece::BR, Piece::NoPiece),
                            Move::new(A8, A5, Piece::BR, Piece::NoPiece),
                            Move::new(A8, A4, Piece::BR, Piece::NoPiece),
                            Move::new(A8, A3, Piece::BR, Piece::NoPiece),
                            Move::new(B8, A7, Piece::BB, Piece::NoPiece),
                            Move::new(B8, C7, Piece::BB, Piece::NoPiece),
                            Move::new(B8, D6, Piece::BB, Piece::NoPiece),
                            Move::new(B8, E5, Piece::BB, Piece::NoPiece),
                            Move::new(B8, F4, Piece::BB, Piece::NoPiece),
                            Move::new(B8, G3, Piece::BB, Piece::NoPiece),
                            Move::new(G1, E2, Piece::BN, Piece::NoPiece),
                            Move::new(G1, F3, Piece::BN, Piece::NoPiece),
                            Move::new(G1, H3, Piece::BN, Piece::NoPiece),
                            Move::new(B5, A4, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, A6, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, A5, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, B6, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, B7, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, B4, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, B3, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, B2, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, C4, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, D3, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, E2, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, F1, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, C6, Piece::BQ, Piece::NoPiece),
                            Move::new(B5, D7, Piece::BQ, Piece::NoPiece),
                            Move::new(H6, G7, Piece::BB, Piece::NoPiece),
                            Move::new(H6, F8, Piece::BB, Piece::NoPiece),
                            Move::new(H6, G5, Piece::BB, Piece::NoPiece),
                            Move::new(H6, F4, Piece::BB, Piece::NoPiece),
                            Move::new(H6, E3, Piece::BB, Piece::NoPiece),
                            Move::new(E8, D8, Piece::BK, Piece::NoPiece),
                            Move::new(E8, F8, Piece::BK, Piece::NoPiece),
                            Move::new(E8, D7, Piece::BK, Piece::NoPiece),
                            Move::new(E8, E7, Piece::BK, Piece::NoPiece),
                            Move::new(E8, F7, Piece::BK, Piece::NoPiece)));
    });

    chess_test!(test_add_castles, {
        let test_case = |fen, expect: &mut Vec<Move>| {
            let pos = Position::from_fen(fen);
            expect.sort_by_key(|m| { m.as_u32() });
            let mut moves = Vec::new();
            add_castles(&pos, &mut moves);
            moves.sort_by_key(|m| { m.as_u32() });
            assert_eq!(*expect, moves);
        };
        test_case("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1",
                  &mut vec!(Move::new_castle(E1, H1, Piece::WK),
                            Move::new_castle(E1, A1, Piece::WK)));
        test_case("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1",
                  &mut vec!(Move::new_castle(E1, H1, Piece::WK),
                            Move::new_castle(E1, A1, Piece::WK)));
        test_case("4k3/8/8/8/8/8/8/R3K2R w Kq - 0 1",
                  &mut vec!(Move::new_castle(E1, H1, Piece::WK)));
        test_case("r3k2r/8/8/8/8/8/8/R3K2R b kq - 0 1",
                  &mut vec!(Move::new_castle(E8, H8, Piece::BK),
                            Move::new_castle(E8, A8, Piece::BK)));
        test_case("r3k2r/8/8/2q5/8/8/8/R3K2R w KQkq - 0 1", &mut Vec::new());
    });

    chess_test!(test_add_pawn_moves, {
        let test_case = |fen, expect: &mut Vec<Move>| {
            let pos = Position::from_fen(fen);
            expect.sort_by_key(|m| { m.as_u32() });
            let mut moves = Vec::new();
            add_pawn_moves(&pos, &mut moves);
            moves.sort_by_key(|m| { m.as_u32() });
            assert_eq!(*expect, moves);
        };
        test_case("4knn1/P5P1/3p4/PpP5/6n1/5b2/4PPP1/4K3 w - b6 0 1",
                  &mut vec!(Move::new(E2, F3, Piece::WP, Piece::BB),
                            Move::new(G2, F3, Piece::WP, Piece::BB),
                            Move::new(C5, D6, Piece::WP, Piece::BP),
                            Move::new_en_passant(A5, B6, Piece::WP, Piece::BP),
                            Move::new_en_passant(C5, B6, Piece::WP, Piece::BP),
                            Move::new(A5, A6, Piece::WP, Piece::NoPiece),
                            Move::new(C5, C6, Piece::WP, Piece::NoPiece),
                            Move::new(E2, E3, Piece::WP, Piece::NoPiece),
                            Move::new(E2, E4, Piece::WP, Piece::NoPiece),
                            Move::new(G2, G3, Piece::WP, Piece::NoPiece),
                            Move::new_promotion(G7, F8, Piece::WP, Piece::BN, PieceType::Rook),
                            Move::new_promotion(G7, F8, Piece::WP, Piece::BN, PieceType::Bishop),
                            Move::new_promotion(G7, F8, Piece::WP, Piece::BN, PieceType::Knight),
                            Move::new_promotion(G7, F8, Piece::WP, Piece::BN, PieceType::Queen),
                            Move::new_promotion(A7, A8, Piece::WP, Piece::NoPiece, PieceType::Rook),
                            Move::new_promotion(A7, A8, Piece::WP, Piece::NoPiece, PieceType::Bishop),
                            Move::new_promotion(A7, A8, Piece::WP, Piece::NoPiece, PieceType::Knight),
                            Move::new_promotion(A7, A8, Piece::WP, Piece::NoPiece, PieceType::Queen)));
        test_case("4k3/p1pp1pb1/1n2pnp1/3PN3/Pp2P3/2N2Q1p/1PPBBPPP/R3K2R b KQ a3 0 1",
                  &mut vec!(Move::new_en_passant(B4, A3, Piece::BP, Piece::WP),
                            Move::new(B4, C3, Piece::BP, Piece::WN),
                            Move::new(H3, G2, Piece::BP, Piece::WP),
                            Move::new(E6, D5, Piece::BP, Piece::WP),
                            Move::new(A7, A6, Piece::BP, Piece::NoPiece),
                            Move::new(A7, A5, Piece::BP, Piece::NoPiece),
                            Move::new(B4, B3, Piece::BP, Piece::NoPiece),
                            Move::new(C7, C5, Piece::BP, Piece::NoPiece),
                            Move::new(C7, C6, Piece::BP, Piece::NoPiece),
                            Move::new(D7, D6, Piece::BP, Piece::NoPiece),
                            Move::new(G6, G5, Piece::BP, Piece::NoPiece)));
    });
}
