
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
    pos->hash ^= piece_hash(piece, square);
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
    pos->endgame_piece_square_eval[color] +=
        endgame_piece_square_value(piece, square);
}

/*
 * Modify |pos| by removing any occupant from |square|.
 */
void remove_piece(position_t* pos, const square_t square)
{
    assert(pos->board[square]);
    piece_t piece = pos->board[square]->piece;
    color_t color = piece_color(piece);
    piece_type_t type = piece_type(piece);
    pos->hash ^= piece_hash(piece, square);
    // Swap the piece at the end of pos->pieces with the removed piece
    int end_index = --pos->piece_count[color][type];
    pos->board[square]->piece = pos->pieces[color][type][end_index].piece;
    pos->board[square]->location = pos->pieces[color][type][end_index].location;
    pos->board[pos->board[square]->location] = pos->board[square];
    pos->board[square] = NULL;
    pos->material_eval[color] -= material_value(piece);
    pos->piece_square_eval[color] -= piece_square_value(piece, square);
    pos->endgame_piece_square_eval[color] -=
        endgame_piece_square_value(piece, square);
}

/*
 * Modify |pos| by moving the occupant of |from| to |to|. If there is already
 * a piece at |to|, it is removed.
 */
void transfer_piece(position_t* pos, const square_t from, const square_t to)
{
    assert(pos->board[from]);
    if (pos->board[to]) {
        remove_piece(pos, to);
    }
    pos->board[to] = pos->board[from];
    pos->board[from] = NULL;
    pos->board[to]->location = to;
    piece_t p = pos->board[to]->piece;
    pos->piece_square_eval[piece_color(p)] -= piece_square_value(p, from);
    pos->piece_square_eval[piece_color(p)] += piece_square_value(p, to);
    pos->endgame_piece_square_eval[piece_color(p)] -=
        endgame_piece_square_value(p, from);
    pos->endgame_piece_square_eval[piece_color(p)] +=
        endgame_piece_square_value(p, to);
    pos->hash ^= piece_hash(p, from);
    pos->hash ^= piece_hash(p, to);
}

/*
 * Modify |pos| by performing the given |move|. The information needed to undo
 * this move is preserved in |undo|.
 */
void do_move(position_t* pos, const move_t move, undo_info_t* undo)
{
    check_move_validity(pos, move);
    check_board_validity(pos);
    // Set undo info, so we can roll back later.
    undo->is_check = pos->is_check;
    undo->prev_move = pos->prev_move;
    undo->ep_square = pos->ep_square;
    undo->fifty_move_counter = pos->fifty_move_counter;
    undo->castle_rights = pos->castle_rights;
    undo->hash = pos->hash;

    // xor old data out of the hash key
    pos->hash ^= ep_hash(pos);
    pos->hash ^= castle_hash(pos);
    pos->hash ^= side_hash(pos);

    const color_t side = pos->side_to_move;
    const color_t other_side = side^1;
    const square_t from = get_move_from(move);
    const square_t to = get_move_to(move);
    assert(valid_board_index(from) && valid_board_index(to));
    pos->ep_square = EMPTY;
    if (piece_type(get_move_piece(move)) == PAWN) {
        if (relative_pawn_rank[side][square_rank(to)] -
                relative_pawn_rank[side][square_rank(from)] != 1) {
            piece_t opp_pawn = create_piece(other_side, PAWN);
            if ((pos->board[to-1] && pos->board[to-1]->piece == opp_pawn) ||
                    (pos->board[to+1] && pos->board[to+1]->piece == opp_pawn)) {
                pos->ep_square = from + pawn_push[side];
            }
        }
        pos->fifty_move_counter = 0;
    } else if (get_move_capture(move) != EMPTY) {
        pos->fifty_move_counter = 0;
    }
    
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

    square_t king_square = pos->pieces[other_side][KING][0].location;
    pos->is_check = is_square_attacked(pos, king_square, side, true);
    ++pos->fifty_move_counter;
    pos->hash_history[pos->ply++] = undo->hash;
    pos->side_to_move ^= 1;
    pos->prev_move = move;
    pos->hash ^= ep_hash(pos);
    pos->hash ^= castle_hash(pos);
    pos->hash ^= side_hash(pos);
    check_board_validity(pos);
}

/*
 * Undo the effects of |move| on |pos|, using the undo info generated by
 * do_move().
 */
void undo_move(position_t* pos, const move_t move, undo_info_t* undo)
{
    check_board_validity(pos);
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
    pos->is_check = undo->is_check;
    pos->ep_square = undo->ep_square;
    pos->fifty_move_counter = undo->fifty_move_counter;
    pos->castle_rights = undo->castle_rights;
    pos->prev_move = undo->prev_move;
    pos->hash = undo->hash;
    check_board_validity(pos);
}

/*
 * Perform a nullmove, aka pass. Not a legal move, but useful for search
 * heuristics.
 */
void do_nullmove(position_t* pos, undo_info_t* undo)
{
    assert(!pos->is_check);
    check_board_validity(pos);
    undo->ep_square = pos->ep_square;
    undo->castle_rights = pos->castle_rights;
    undo->prev_move = pos->prev_move;
    undo->fifty_move_counter = pos->fifty_move_counter;
    undo->hash = pos->hash;
    undo->is_check = pos->is_check;
    pos->hash ^= ep_hash(pos);
    pos->hash ^= side_hash(pos);
    pos->side_to_move ^= 1;
    pos->hash ^= side_hash(pos);
    pos->ep_square = EMPTY;
    pos->fifty_move_counter++;
    pos->hash_history[pos->ply++] = undo->hash;
    pos->prev_move = NULL_MOVE;
    check_board_validity(pos);
}

/*
 * Undo the effects of a nullmove.
 */
void undo_nullmove(position_t* pos, undo_info_t* undo)
{
    assert(!pos->is_check);
    check_board_validity(pos);
    pos->ep_square = undo->ep_square;
    pos->castle_rights = undo->castle_rights;
    pos->prev_move = undo->prev_move;
    pos->side_to_move ^= 1;
    pos->fifty_move_counter = undo->fifty_move_counter;
    pos->hash = undo->hash;
    pos->is_check = undo->is_check;
    pos->ply--;
    check_board_validity(pos);
}

