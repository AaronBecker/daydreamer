
#include "daydreamer.h"

static int generate_queen_promotions(const position_t* pos, move_t* moves);
static int generate_evasions(const position_t* pos, move_t* moves);
static void generate_pawn_captures(const position_t* pos,
        square_t from,
        piece_t piece,
        move_t** moves);
static void generate_pawn_noncaptures(const position_t* pos,
        square_t from,
        piece_t piece,
        move_t** moves);
static void generate_piece_captures(const position_t* pos,
        square_t from,
        piece_t piece,
        move_t** moves);
static void generate_piece_noncaptures(const position_t* pos,
        square_t from,
        piece_t piece,
        move_t** moves);


/*
 * Push a new move onto a stack of moves, first doing some sanity checks.
 */
static move_t* add_move(const position_t* pos,
        const move_t move,
        move_t* moves)
{
    (void)pos; // avoid warning when NDEBUG is defined
    check_move_validity(pos, move);
    *(moves++) = move;
    return moves;
}

/*
 * Fill the provided list with all legal moves in the given position.
 */
int generate_legal_moves(const position_t* pos, move_t* moves)
{
    if (is_check(pos)) return generate_evasions(pos, moves);
    int num_pseudo = generate_pseudo_moves(pos, moves);
    move_t* moves_tail = moves+num_pseudo;
    move_t* moves_curr = moves;
    while (moves_curr < moves_tail) {
        if (!is_move_legal((position_t*)pos, *moves_curr)) {
            *moves_curr = *(--moves_tail);
            *moves_tail = 0;
        } else {
            ++moves_curr;
        }
    }
    return moves_tail-moves;
}

/*
 * Fill the provided list with all legal non-capturing moves in the given
 * position.
 */
int generate_legal_noncaptures(const position_t* pos, move_t* moves)
{
    int num_pseudo;
    if (is_check(pos)) num_pseudo = generate_evasions(pos, moves);
    else num_pseudo = generate_pseudo_noncaptures(pos, moves);
    move_t* moves_tail = moves+num_pseudo;
    move_t* moves_curr = moves;
    while (moves_curr < moves_tail) {
        if (!is_move_legal((position_t*)pos, *moves_curr)) {
            *moves_curr = *(--moves_tail);
            *moves_tail = 0;
        } else {
            ++moves_curr;
        }
    }
    return moves_tail-moves;
}

/*
 * Fill the provided list with all pseudolegal moves in the given position.
 * Pseudolegal moves are moves which would be legal if we didn't have to worry
 * about leaving our king in check.
 */
int generate_pseudo_moves(const position_t* pos, move_t* moves)
{
    if (is_check(pos)) return generate_evasions(pos, moves);
    move_t* moves_head = moves;
    moves += generate_pseudo_captures(pos, moves);
    moves += generate_pseudo_noncaptures(pos, moves);
    return moves-moves_head;
}

/*
 * Generate all pseudolegal moves that should be considered during quiescence
 * search. Currently this is just captures and promotions to queen.
 */
int generate_quiescence_moves(const position_t* pos,
        move_t* moves,
        bool generate_checks)
{
    if (is_check(pos)) return generate_evasions(pos, moves);
    move_t* moves_head = moves;
    // TODO: more thorough testing/validation before this can be turned back on
    // moves += generate_queen_promotions(pos, moves);
    moves += generate_pseudo_captures(pos, moves);
    //if (generate_checks) moves += generate_pseudo_checks(pos, moves);
    return moves-moves_head;
}



/*
 * Fill the provided list with all pseudolegal captures in the given position.
 */
int generate_pseudo_captures(const position_t* pos, move_t* moves)
{
    move_t* moves_head = moves;
    color_t side = pos->side_to_move;
    for (piece_type_t type = KING; type != NONE; --type) {
        piece_t piece = create_piece(side, type);
        for (int i = 0; i < pos->piece_count[side][type]; ++i) {
            square_t from = pos->pieces[side][type][i];
            switch (piece_type(piece)) {
                case PAWN: generate_pawn_captures(pos, from, piece, &moves);
                           break;
                case KING:
                case KNIGHT:
                case BISHOP:
                case ROOK:
                case QUEEN: generate_piece_captures(pos, from, piece, &moves);
                            break;
                default:    assert(false);
            }
        }
    }
    *moves = 0;
    return moves-moves_head;
}

/*
 * Fill the provided list with all pseudolegal non-capturing moves in the given
 * position.
 */
int generate_pseudo_noncaptures(const position_t* pos, move_t* moves)
{
    move_t* moves_head = moves;
    color_t side = pos->side_to_move;
    for (piece_type_t type = KING; type != NONE; --type) {
        piece_t piece = create_piece(side, type);
        for (int i = 0; i < pos->piece_count[side][type]; ++i) {
            square_t from = pos->pieces[side][type][i];
            switch (piece_type(piece)) {
                case PAWN:
                    generate_pawn_noncaptures(pos, from, piece, &moves);
                    break;
                case KING:
                case KNIGHT:
                case BISHOP:
                case ROOK:
                case QUEEN:
                    generate_piece_noncaptures(pos, from, piece, &moves);
                    break;
                default: assert(false);
            }
        }
    }

    // Castling. Castles are considered pseudo-legal if we have appropriate
    // castling rights and the squares between king and rook are unoccupied.
    // Note: this would require some overhauling to support Chess960.
    square_t king_home = E1 + side*A8;
    if (has_oo_rights(pos, side) &&
            pos->board[king_home+1] == EMPTY &&
            pos->board[king_home+2] == EMPTY) {
        moves = add_move(pos,
                create_move_castle(king_home, king_home+2,
                    create_piece(side, KING)),
                moves);
    }
    if (has_ooo_rights(pos, side) &&
            pos->board[king_home-1] == EMPTY &&
            pos->board[king_home-2] == EMPTY &&
            pos->board[king_home-3] == EMPTY) {
        moves = add_move(pos,
                create_move_castle(king_home, king_home-2,
                    create_piece(side, KING)),
                moves);
    }
    *moves = 0;
    return (moves-moves_head);
}

/*
 * Add all pseudo-legal non-capturing promotions to queen. Used for
 * quiescence search.
 */
static int generate_queen_promotions(const position_t* pos, move_t* moves)
{
    move_t* moves_head = moves;
    color_t side = pos->side_to_move;
    piece_t piece = create_piece(side, PAWN);
    for (int i = 0; i < pos->piece_count[side][PAWN]; ++i) {
        square_t from = pos->pieces[side][PAWN][i];
        rank_t relative_rank = relative_pawn_rank[side][square_rank(from)];
        if (relative_rank < RANK_7) continue;
        square_t to = from + pawn_push[side];
        if (pos->board[to]) continue;
        moves = add_move(pos,
                create_move_promote(from, to, piece, EMPTY, QUEEN),
                moves);
    }
    *moves = 0;
    return (moves-moves_head);
}

static direction_t pin_direction(const position_t* pos,
        square_t from,
        square_t king_sq)
{
    // Figure out whether or not we can discover check by moving.
    direction_t pin_dir = 0;
    if (!possible_attack(from, king_sq, WQ)) return 0;
    direction_t king_dir = direction(from, king_sq);
    square_t sq;
    for (sq = from + king_dir;
            valid_board_index(sq) && pos->board[sq] == EMPTY;
            sq += king_dir) {}
    if (sq == king_sq) {
        // Nothing between us and the king. Is there anything
        // behind us that's doing the pinning?
        for (sq = from - king_dir;
                valid_board_index(sq) && pos->board[sq] == EMPTY;
                sq -= king_dir) {}
        if (valid_board_index(sq) &&
                piece_colors_differ(pos->board[sq], pos->board[from]) &&
                possible_attack(from, king_sq, pos->board[sq]) &&
                (piece_slide_type(pos->board[sq]) != NONE)) {
            pin_dir = king_dir;
        }
    }
    return pin_dir;
}

/*
 * Generate all moves that evade check in the given position.
 */
int generate_evasions(const position_t* pos, move_t* moves)
{
    assert(pos->is_check && pos->board[pos->check_square]);
    move_t* moves_head = moves;
    color_t side = pos->side_to_move, other_side = side^1;
    square_t king_sq = pos->pieces[side][KING][0];
    square_t check_sq = pos->check_square;
    piece_t king = create_piece(side, KING);
    piece_t checker = pos->board[check_sq];

    // Generate King moves.
    // Don't let the king mask its possible destination squares in calls
    // to is_square_attacked.
    square_t from = king_sq, to = INVALID_SQUARE;
    ((position_t*)pos)->board[king_sq] = EMPTY;
    for (const direction_t* delta = piece_deltas[king]; *delta; ++delta) {
        to = from + *delta;
        if (!valid_board_index(to)) continue;
        piece_t capture = pos->board[to];
        if (capture != EMPTY && !piece_colors_differ(king, capture)) continue;
        if (is_square_attacked((position_t*)pos,to,other_side)) continue;
        // TODO: can this be eliminated now?
        ((position_t*)pos)->board[king_sq] = king;
        moves = add_move(pos, create_move(from, to, king, capture), moves);
        ((position_t*)pos)->board[king_sq] = EMPTY;
    }
    ((position_t*)pos)->board[king_sq] = king;
    // If there are multiple checkers, only king moves are possible.
    if (pos->is_check > 1) {
        *moves = 0;
        return moves-moves_head;
    }
    
    // First, the most common case: a check can be evaded via an
    // en passant capture. Note that if we're in check and an en passant
    // capture is available, the only way an en passant capture would evade
    // the check is if it's the newly moved pawn that's checking us.
    direction_t pin_dir;
    if (pos->ep_square != EMPTY && check_sq+pawn_push[side] == pos->ep_square) {
        piece_t our_pawn = create_piece(side, PAWN);
        to = pos->ep_square;
        for (int i=0; i<2; ++i) {
            from = to-piece_deltas[our_pawn][i];
            if (pos->board[from] && pos->board[from] == our_pawn) {
                pin_dir = pin_direction(pos, from, king_sq);
                if (pin_dir) continue;
                moves = add_move(pos,
                        create_move_enpassant(from, to, our_pawn, checker),
                        moves);
            }
        }
    }
    // Generate captures of the checker.
    for (piece_type_t type = QUEEN; type != NONE; --type) {
        for (int i = 0; i < pos->piece_count[side][type]; ++i) {
            from = pos->pieces[side][type][i];
            piece_t piece = create_piece(side, type);
            pin_dir = pin_direction(pos, from, king_sq);
            if (pin_dir) continue;
            if (!possible_attack(from, check_sq, piece)) continue;
            if (piece_slide_type(piece) == NONE) {
                if (type == PAWN && relative_pawn_rank
                        [side][square_rank(from)] == RANK_7) {
                    // Capture and promote.
                    for (piece_t promoted=QUEEN; promoted > PAWN; --promoted) {
                        moves = add_move(pos,
                                create_move_promote(from, check_sq, piece,
                                    checker, promoted), moves);
                    }
                } else {
                    moves = add_move(pos,
                            create_move(from, check_sq, piece, checker),
                            moves);
                }
            } else {
                // A sliding piece, keep going until we hit something.
                direction_t check_dir = direction(from, check_sq);
                for (to = from + check_dir;
                        valid_board_index(to) && !pos->board[to];
                        to += check_dir) {}
                if (to == check_sq) {
                    moves = add_move(pos,
                            create_move(from, to, piece, checker),
                            moves);
                    continue;
                }
            }
        }
    }
    if (piece_slide_type(checker) == NONE) {
        *moves = 0;
        return moves-moves_head;
    }
    
    // A slider is doing the checking; generate blocking moves.
    direction_t block_dir =
        get_attack_data(check_sq, king_sq).relative_direction;
    for (piece_type_t type = QUEEN; type != NONE; --type) {
        for (int i = 0; i < pos->piece_count[side][type]; ++i) {
            from = pos->pieces[side][type][i];
            piece_t piece = create_piece(side, type);
            direction_t k_dir;
            pin_dir = pin_direction(pos, from, king_sq);
            if (pin_dir) continue;
            if (type == PAWN) {
                to = from + pawn_push[side];
                if (pos->board[to]) continue;
                rank_t rank = relative_pawn_rank[side][square_rank(from)];
                k_dir = get_attack_data(to, king_sq).relative_direction;
                if (k_dir == block_dir &&
                        ((king_sq < to && check_sq > to) ||
                         (king_sq > to && check_sq < to))) {
                    if (rank == RANK_7) {
                        // Block and promote.
                        for (piece_t promoted=QUEEN;
                                promoted > PAWN; --promoted) {
                            moves = add_move(pos,
                                    create_move_promote(from, to, piece,
                                        EMPTY, promoted), moves);
                        }
                    } else {
                        moves = add_move(pos,
                                create_move(from, to, piece, EMPTY),
                                moves);
                    }
                }
                if (rank != RANK_2) continue;
                to += pawn_push[side];
                if (pos->board[to]) continue;
                k_dir = get_attack_data(to, king_sq).relative_direction;
                if (k_dir == block_dir &&
                        ((king_sq < to && check_sq > to) ||
                         (king_sq > to && check_sq < to))) {
                    moves = add_move(pos,
                            create_move(from, to, piece, EMPTY),
                            moves);
                }
            } else if (type == KNIGHT) {
                for (const direction_t* delta = piece_deltas[piece];
                        *delta; ++delta) {
                    to = from + *delta;
                    if (!valid_board_index(to) || pos->board[to]) continue;
                    k_dir = get_attack_data(to, king_sq).relative_direction;
                    if (k_dir == block_dir &&
                            ((king_sq < to && check_sq > to) ||
                             (king_sq > to && check_sq < to))) {
                        moves = add_move(pos,
                                create_move(from, to, piece, EMPTY),
                                moves);
                    }
                }
            } else {
               for (const direction_t* delta = piece_deltas[piece];
                        *delta; ++delta) {
                    for (to = from + *delta;
                            valid_board_index(to) && !pos->board[to];
                            to += *delta) {
                        k_dir = get_attack_data(to, king_sq).relative_direction;
                        if (k_dir == block_dir &&
                                ((king_sq < to && check_sq > to) ||
                                 (king_sq > to && check_sq < to))) {
                            moves = add_move(pos,
                                    create_move(from, to, piece, EMPTY),
                                    moves);
                            break;
                        }
                    }
                }
            }
        }
    }
    *moves = 0;
    return moves-moves_head;
}

/*
 * Generate all non-capturing, non-promoting, pseudo-legal checks. Used for
 * quiescent move generation.
 * FIXME: untested and unused so far.
 */
int generate_pseudo_checks(const position_t* pos, move_t* moves)
{
    move_t* moves_head = moves;
    color_t side = pos->side_to_move, other_side = side^1;
    square_t king_sq = pos->pieces[other_side][KING][0];
    for (piece_type_t type = QUEEN; type != NONE; --type) {
        for (int i = 0; i < pos->piece_count[side][type]; ++i) {
            square_t to = INVALID_SQUARE;
            square_t from = pos->pieces[side][type][i];
            piece_t piece = create_piece(side, type);
            // Figure out whether or not we can discover check by moving.
            direction_t discover_check_dir = 0;
            bool will_discover_check;
            const attack_data_t* king_atk = &get_attack_data(from, king_sq);
            if ((king_atk->possible_attackers & Q_FLAG) == 0) {
                discover_check_dir = 0;
            } else {
                direction_t king_dir = king_atk->relative_direction;
                square_t sq;
                for (sq = from + king_dir;
                        valid_board_index(sq) && !pos->board[sq];
                        sq += king_dir) {}
                if (sq == king_sq) {
                    // Nothing between us and the king. Is there anything
                    // behind us to do the check when we move?
                    for (sq = from - king_dir;
                            valid_board_index(sq) && !pos->board[sq];
                            sq -= king_dir) {}
                    if (valid_board_index(sq) &&
                            (side == piece_color(pos->board[sq])) &&
                            (king_atk->possible_attackers &
                             get_piece_flag(pos->board[sq]))) {
                        discover_check_dir = king_dir;
                    }
                }
            }
            // Generate checking moves.
            if (type == PAWN) {
                will_discover_check = discover_check_dir &&
                    abs(discover_check_dir) != N;
                to = from + pawn_push[side];
                if (pos->board[to]) continue;
                rank_t relative_rank =
                    relative_pawn_rank[side][square_rank(from)];
                if (relative_rank == RANK_7) continue; // non-promotes only
                for (const direction_t* delta = piece_deltas[piece];
                        *delta; ++delta) {
                    if (will_discover_check || to + *delta == king_sq) {
                        moves = add_move(pos,
                                create_move(from, to, piece, EMPTY),
                                moves);
                        break;
                    }
                }
                to += pawn_push[side];
                if (relative_rank == RANK_2 && !pos->board[to]) {
                    for (const direction_t* delta = piece_deltas[piece];
                            *delta; ++delta) {
                        if (will_discover_check || to + *delta == king_sq) {
                            moves = add_move(pos,
                                    create_move(from, to, piece, EMPTY),
                                    moves);
                            break;
                        }
                    }
                }
            } else if (type == KNIGHT) {
                for (const direction_t* delta = piece_deltas[piece];
                        *delta; ++delta) {
                    to = from + *delta;
                    if (!valid_board_index(to) || pos->board[to]) continue;
                    if (discover_check_dir ||
                            possible_attack(to, king_sq, WN)) {
                        moves = add_move(pos,
                                create_move(from, to, piece, EMPTY),
                                moves);
                    }
                }
            } else {
                for (const direction_t* delta = piece_deltas[piece];
                        *delta; ++delta) {
                    for (to = from + *delta;
                            valid_board_index(to) && !pos->board[to];
                            to += *delta) {
                        will_discover_check = discover_check_dir &&
                            abs(discover_check_dir) != abs(*delta);
                        if (will_discover_check) {
                            moves = add_move(pos,
                                    create_move(from, to, piece, NONE),
                                    moves);
                            continue;
                        }
                        if (possible_attack(to, king_sq, piece)) {
                            const direction_t to_king = direction(to, king_sq);
                            for (square_t x=to + to_king;
                                    valid_board_index(x); x += to_king) {
                                if (x == king_sq) {
                                    moves = add_move(pos,
                                            create_move(from, to, piece, NONE),
                                            moves);
                                    break;
                                }
                                if (pos->board[x]) break;
                            }
                        }
                    }
                }
            }
        }
    }
    *moves = 0;
    return moves-moves_head;
}

/*
 * Add all pseudo-legal captures that the given pawn can make.
 */
static void generate_pawn_captures(const position_t* pos,
        square_t from,
        piece_t piece,
        move_t** moves_head)
{
    color_t side = pos->side_to_move;
    square_t to;
    rank_t relative_rank = relative_pawn_rank[side][square_rank(from)];
    move_t* moves = *moves_head;
    if (relative_rank < RANK_7) {
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            // non-promote captures
            to = from + *delta;
            if (to == pos->ep_square) {
                moves = add_move(pos, create_move_enpassant(from, to, piece,
                        pos->board[to + pawn_push[side^1]]), moves);
            }
            if (!valid_board_index(to) || pos->board[to] == EMPTY) continue;
            if (piece_colors_differ(piece, pos->board[to])) {
                moves = add_move(pos, create_move(from, to, piece,
                        pos->board[to]), moves);
            }
        }
    } else {
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            // capture/promotes
            to = from + *delta;
            if (!valid_board_index(to) ||
                !pos->board[to] ||
                !piece_colors_differ(piece, pos->board[to])) continue;
            for (piece_t promoted=QUEEN; promoted > PAWN; --promoted) {
                moves = add_move(pos, create_move_promote(from, to, piece,
                        pos->board[to], promoted), moves);
            }
        }
    }
    *moves_head = moves;
}

/*
 * Add all pseudo-legal non-capturing moves that the given pawn can make.
 */
static void generate_pawn_noncaptures(const position_t* pos,
        square_t from,
        piece_t piece,
        move_t** moves_head)
{
    color_t side = pos->side_to_move;
    square_t to;
    rank_t relative_rank = relative_pawn_rank[side][square_rank(from)];
    move_t* moves = *moves_head;
    if (relative_rank < RANK_7) {
        // non-promotions
        to = from + pawn_push[side];
        if (pos->board[to]) return;
        moves = add_move(pos, create_move(from, to, piece, EMPTY), moves);
        to += pawn_push[side];
        if (relative_rank == RANK_2 && pos->board[to] == EMPTY) {
            // initial two-square push
            moves = add_move(pos,
                    create_move(from, to, piece, EMPTY),
                    moves);
        }
    } else {
        // promotions
        to = from + pawn_push[side];
        if (pos->board[to] != EMPTY) return;
        for (piece_t promoted=QUEEN; promoted > PAWN; --promoted) {
            moves = add_move(pos, create_move_promote(from, to, piece,
                        EMPTY, promoted), moves);
        }
    }
    *moves_head = moves;
}

/*
 * Add all pseudo-legal captures that the given piece (N/B/Q/K) can make.
 */
static void generate_piece_captures(const position_t* pos,
        square_t from,
        piece_t piece,
        move_t** moves_head)
{
    move_t* moves = *moves_head;
    square_t to;
    if (piece_slide_type(piece) == NONE) {
        // not a sliding piece, just iterate over dest. squares
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            to = from + *delta;
            if (!valid_board_index(to) || !pos->board[to]) continue;
            if (piece_colors_differ(piece, pos->board[to])) {
                moves = add_move(pos, create_move(from, to, piece,
                            pos->board[to]), moves);
            }
        }
    } else {
        // a sliding piece, keep going until we hit something
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            to = from;
            do {
                to += *delta;
                if (!valid_board_index(to)) break;
                if (!pos->board[to]) continue;
                if (piece_colors_differ(piece, pos->board[to])) {
                    moves = add_move(pos, create_move(from, to, piece,
                            pos->board[to]), moves);
                }
            } while (!pos->board[to]);
        }
    }
    *moves_head = moves;
}

/*
 * Add all pseudo-legal non-capturing moves that the given piece (N/B/Q/K)
 * can make, with the exception of castles.
 */
static void generate_piece_noncaptures(const position_t* pos,
        square_t from,
        piece_t piece,
        move_t** moves_head)
{
    move_t* moves = *moves_head;
    square_t to;
    if (piece_slide_type(piece) == NONE) {
        // not a sliding piece, just iterate over dest. squares
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            to = from + *delta;
            if (!valid_board_index(to) || pos->board[to]) continue;
            moves = add_move(pos,
                    create_move(from, to, piece, NONE),
                    moves);
        }
    } else {
        // a sliding piece, keep going until we hit something
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            to = from;
            do {
                to += *delta;
                if (!valid_board_index(to) || pos->board[to]) break;
                moves = add_move(pos,
                        create_move(from, to, piece, NONE),
                        moves);
            } while (!pos->board[to]);
        }
    }
    *moves_head = moves;
}

