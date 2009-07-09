
#include <assert.h>
#include "daydreamer.h"

/*
 * Modify |pos| by adding |piece| to |square|. If |square| is occupied, its
 * occupant is properly removed.
 */
void place_piece(position_t* pos, const piece_t piece, const square_t square)
{
    if (pos->board[square]) {
        remove_piece(pos, square);
    }
    color_t color = piece_color(piece);
    piece_type_t type = piece_type(piece);
    assert(color == WHITE || color == BLACK);
    assert(type != INVALID_PIECE);
    assert(square != INVALID_SQUARE);

    piece_entry_t* entry = &pos->pieces[color][type]
        [pos->piece_count[color][type]++];
    entry->location = square;
    entry->piece = piece;
    pos->board[square] = entry;
    pos->material_eval[color] += material_value(piece);
    pos->piece_square_eval[color] += piece_square_value(piece, square);
}

/*
 * Modify |pos| by removing any occupant from |square|.
 */
void remove_piece(position_t* pos, const square_t square)
{
    if (!pos->board[square]) return;
    piece_t piece = pos->board[square]->piece;
    color_t color = piece_color(piece);
    piece_type_t type = piece_type(piece);
    // Swap the piece at the end of pos->pieces with the removed piece
    int end_index = --pos->piece_count[color][type];
    pos->board[square]->piece = pos->pieces[color][type][end_index].piece;
    pos->board[square]->location = pos->pieces[color][type][end_index].location;
    pos->board[pos->board[square]->location] = pos->board[square];
    pos->board[square] = NULL;
    pos->material_eval[color] -= material_value(piece);
    pos->piece_square_eval[color] -= piece_square_value(piece, square);
}

/*
 * Modify |pos| by moving the occupant of |from| to |to|. If there is already
 * a piece at |to|, it is removed.
 */
void transfer_piece(position_t* pos, const square_t from, const square_t to)
{
    if (!pos->board[from]) return;
    if (pos->board[to]) {
        remove_piece(pos, to);
    }
    pos->board[to] = pos->board[from];
    pos->board[from] = NULL;
    pos->board[to]->location = to;
    piece_t p = pos->board[to]->piece;
    pos->piece_square_eval[piece_color(p)] -= piece_square_value(p, from);
    pos->piece_square_eval[piece_color(p)] += piece_square_value(p, to);
}

/*
 * Modify |pos| by performing the given |move|. The information needed to undo
 * this move is preserved in |undo|.
 */
void do_move(position_t* pos, const move_t move, undo_info_t* undo)
{
    // Set undo info, so we can roll back later.
    undo->prev_move = pos->prev_move;
    undo->ep_square = pos->ep_square;
    undo->fifty_move_counter = pos->fifty_move_counter;
    undo->castle_rights = pos->castle_rights;

    const color_t side = pos->side_to_move;
    const color_t other_side = side^1;
    const square_t from = get_move_from(move);
    const square_t to = get_move_to(move);
    assert(valid_board_index(from) && valid_board_index(to));
    pos->ep_square = EMPTY;
    if (piece_type(get_move_piece(move)) == PAWN) {
        if (relative_pawn_rank[side][square_rank(to)] -
                relative_pawn_rank[side][square_rank(from)] != 1) {
            pos->ep_square = from + pawn_push[side];
        }
        pos->fifty_move_counter = 0;
    } else if (get_move_capture(move) != EMPTY) pos->fifty_move_counter = 0;
    
    // Remove castling rights as necessary.
    if (from == A1 + side*A8) remove_ooo_rights(pos, side);
    else if (from == H1 + side*A8) remove_oo_rights(pos, side);
    else if (from == E1 + side*A8)  {
        remove_oo_rights(pos, side);
        remove_ooo_rights(pos, side);
    } 
    if (to == A1 + other_side*A8) remove_ooo_rights(pos, other_side);
    else if (to == H1 + other_side*A8) remove_oo_rights(pos, other_side);

    transfer_piece(pos, from, to);

    const piece_type_t promote_type = get_move_promote(move);
    if (is_move_castle_short(move)) {
        transfer_piece(pos, H1 + A8*side, F1 + A8*side);
    } else if (is_move_castle_long(move)) {
        transfer_piece(pos, A1 + A8*side, D1 + A8*side);
    } else if (is_move_enpassant(move)) {
        remove_piece(pos, to-pawn_push[side]);
    } else if (promote_type) {
        place_piece(pos, create_piece(side, promote_type), to);
    }

    pos->ply++;
    pos->side_to_move ^= 1;
    pos->prev_move = move;
}

/*
 * Undo the effects of |move| on |pos|, using the undo info generated by
 * do_move().
 */
void undo_move(position_t* pos, const move_t move, undo_info_t* undo)
{
    const color_t side = pos->side_to_move^1;
    const square_t from = get_move_from(move);
    const square_t to = get_move_to(move);

    // Move the piece back.
    transfer_piece(pos, to, from);
    piece_type_t captured = get_move_capture(move);
    if (captured != EMPTY) {
        if (is_move_enpassant(move)) {
            place_piece(pos, create_piece(side^1, PAWN), to-pawn_push[side]);
        } else {
            place_piece(pos, create_piece(pos->side_to_move, captured), to);
        }
    }

    // Un-promote/castle, if necessary, and fix en passant captures.
    const piece_type_t promote_type = get_move_promote(move);
    if (is_move_castle_short(move)) {
        transfer_piece(pos, F1 + A8*side, H1 + A8*side);
    } else if (is_move_castle_long(move)) {
        transfer_piece(pos, D1 + A8*side, A1 + A8*side);
    } else if (promote_type) {
        place_piece(pos, create_piece(side, PAWN), from);
    }

    // Reset non-board state information.
    pos->side_to_move ^= 1;
    pos->ply--;
    pos->ep_square = undo->ep_square;
    pos->fifty_move_counter = undo->fifty_move_counter;
    pos->castle_rights = undo->castle_rights;
    pos->prev_move = undo->prev_move;
}

void do_nullmove(position_t* pos, undo_info_t* undo)
{
    undo->ep_square = pos->ep_square;
    undo->castle_rights = pos->castle_rights;
    undo->prev_move = pos->prev_move;
    undo->fifty_move_counter = pos->fifty_move_counter;
    pos->side_to_move ^= 1;
    pos->ep_square = EMPTY;
    pos->fifty_move_counter++;
    pos->ply++;
    pos->prev_move = NULL_MOVE;
}

void undo_nullmove(position_t* pos, undo_info_t* undo)
{
    pos->ep_square = undo->ep_square;
    pos->castle_rights = undo->castle_rights;
    pos->prev_move = undo->prev_move;
    pos->fifty_move_counter = undo->fifty_move_counter;
    pos->side_to_move ^= 1;
    pos->ply--;
}

/*
 * Perform some sanity checks on |move| to flag obviously invalid moves.
 */
void check_move_validity(const position_t* pos, const move_t move)
{
    const square_t from = get_move_from(move);
    const square_t to = get_move_to(move);
    const piece_t piece = get_move_piece(move);
    const piece_t capture = get_move_capture(move);
    assert(valid_board_index(from) && valid_board_index(to));
    assert(pos->board[from]->piece == piece);
    assert(pos->board[from]->location == from);
    if (capture && !is_move_enpassant(move)) {
        assert(pos->board[to]->piece == capture);
    } else {
        assert(pos->board[to] == NULL);
    }
}
