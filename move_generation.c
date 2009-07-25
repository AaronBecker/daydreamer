
#include "daydreamer.h"

static void generate_pawn_captures(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves);
static void generate_pawn_noncaptures(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves);
static void generate_piece_captures(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves);
static void generate_piece_noncaptures(const position_t* pos,
        const piece_entry_t* piece_entry,
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
    int num_pseudo = generate_pseudo_noncaptures(pos, moves);
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
    move_t* moves_head = moves;
    moves += generate_pseudo_captures(pos, moves);
    moves += generate_pseudo_noncaptures(pos, moves);
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
        for (int i = 0; i < pos->piece_count[side][type]; ++i) {
            const piece_entry_t* piece_entry = &pos->pieces[side][type][i];
            switch (piece_type(piece_entry->piece)) {
                case PAWN: generate_pawn_captures(pos, piece_entry, &moves);
                           break;
                case KING:
                case KNIGHT:
                case BISHOP:
                case ROOK:
                case QUEEN: generate_piece_captures(pos, piece_entry, &moves);
                            break;
                default:    assert(false);
            }
        }
    }
    *moves = 0;
    return (moves-moves_head);
}


/*
 * Add all pseudo-legal captures that the given piece (N/B/Q/K) can make.
 */
static void generate_piece_captures(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves_head)
{
    const piece_t piece = piece_entry->piece;
    const square_t from = piece_entry->location;
    move_t* moves = *moves_head;
    square_t to;
    if (piece_slide_type(piece) == NONE) {
        // not a sliding piece, just iterate over dest. squares
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            to = from + *delta;
            if (!valid_board_index(to) || !pos->board[to]) continue;
            if (piece_colors_differ(piece, pos->board[to]->piece)) {
                moves = add_move(pos, create_move(from, to, piece,
                            pos->board[to]->piece), moves);
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
                if (piece_colors_differ(piece, pos->board[to]->piece)) {
                    moves = add_move(pos, create_move(from, to, piece,
                            pos->board[to]->piece), moves);
                }
            } while (!pos->board[to]);
        }
    }
    *moves_head = moves;
}

/*
 * Add all pseudo-legal captures that the given pawn can make.
 */
static void generate_pawn_captures(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves_head)
{
    color_t side = pos->side_to_move;
    piece_t piece = piece_entry->piece;
    square_t from = piece_entry->location;
    square_t to;
    rank_t relative_rank = relative_pawn_rank[side][square_rank(from)];
    move_t* moves = *moves_head;
    if (relative_rank < RANK_7) {
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            // non-promote captures
            to = from + *delta;
            if (to == pos->ep_square) {
                moves = add_move(pos, create_move_enpassant(from, to, piece,
                        pos->board[to + pawn_push[side^1]]->piece), moves);
            }
            if (!valid_board_index(to) || !pos->board[to]) continue;
            if (piece_colors_differ(piece, pos->board[to]->piece)) {
                moves = add_move(pos, create_move(from, to, piece,
                        pos->board[to]->piece), moves);
            }
        }
    } else {
        for (const direction_t* delta = piece_deltas[piece]; *delta; ++delta) {
            // capture/promotes
            to = from + *delta;
            if (!valid_board_index(to) ||
                !pos->board[to] ||
                !piece_colors_differ(piece, pos->board[to]->piece)) continue;
            for (piece_t promoted=QUEEN; promoted > PAWN; --promoted) {
                moves = add_move(pos, create_move_promote(from, to, piece,
                        pos->board[to]->piece, promoted), moves);
            }
        }
    }
    *moves_head = moves;
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
        for (int i = 0; i < pos->piece_count[side][type]; ++i) {
            const piece_entry_t* piece_entry = &pos->pieces[side][type][i];
            switch (piece_type(piece_entry->piece)) {
                case PAWN:
                    generate_pawn_noncaptures(pos, piece_entry, &moves);
                    break;
                case KING:
                case KNIGHT:
                case BISHOP:
                case ROOK:
                case QUEEN:
                    generate_piece_noncaptures(pos, piece_entry, &moves);
                    break;
                default: assert(false);
            }
        }
    }

    // Castling. Castles are considered pseudo-legal if we have appropriate
    // castling rights and the squares between king and rook are unoccupied.
    // Note: this would require some overhauling to support Chess960.
    square_t king_home = E1 + side*A8;
    if (has_oo_rights(pos, side) && !pos->board[king_home+1] &&
            !pos->board[king_home+2]) {
        moves = add_move(pos,
                create_move_castle(king_home, king_home+2,
                    create_piece(side, KING)),
                moves);
    }
    if (has_ooo_rights(pos, side) && !pos->board[king_home-1] && 
            !pos->board[king_home-2] && !pos->board[king_home-3]) {
        moves = add_move(pos,
                create_move_castle(king_home, king_home-2,
                    create_piece(side, KING)),
                moves);
    }
    *moves = 0;
    return (moves-moves_head);
}

/*
 * Add all pseudo-legal non-capturing moves that the given pawn can make.
 */
static void generate_pawn_noncaptures(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves_head)
{
    color_t side = pos->side_to_move;
    piece_t piece = piece_entry->piece;
    square_t from = piece_entry->location;
    square_t to;
    rank_t relative_rank = relative_pawn_rank[side][square_rank(from)];
    move_t* moves = *moves_head;
    if (relative_rank < RANK_7) {
        // non-promotions
        to = from + pawn_push[side];
        if (pos->board[to]) return;
        moves = add_move(pos, create_move(from, to, piece, EMPTY), moves);
        to += pawn_push[side];
        if (relative_rank == RANK_2 && pos->board[to] == NULL) {
            // initial two-square push
            moves = add_move(pos,
                    create_move(from, to, piece, EMPTY),
                    moves);
        }
    } else {
        // promotions
        to = from + pawn_push[side];
        if (pos->board[to]) return;
        for (piece_t promoted=QUEEN; promoted > PAWN; --promoted) {
            moves = add_move(pos, create_move_promote(from, to, piece,
                        EMPTY, promoted), moves);
        }
    }
    *moves_head = moves;
}

/*
 * Add all pseudo-legal non-capturing moves that the given piece (N/B/Q/K)
 * can make, with the exception of castles.
 */
static void generate_piece_noncaptures(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves_head)
{
    const piece_t piece = piece_entry->piece;
    const square_t from = piece_entry->location;
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

