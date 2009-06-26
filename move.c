
#include <assert.h>
#include "grasshopper.h"

void place_piece(position_t* pos, const piece_t piece, const square_t square)
{
    if (pos->board[square]) {
        remove_piece(pos, square);
    }
    color_t color = piece_color(piece);
    piece_type_t type = piece_type(piece);
    assert((color == WHITE || color == BLACK) && type != INVALID_PIECE);

    piece_entry_t* entry = &pos->pieces[color][type]
        [pos->piece_count[color][type]++];
    entry->location = square;
    entry->piece = piece;
    pos->board[square] = entry;
}

void remove_piece(position_t* pos, const square_t square)
{
    piece_t piece = pos->board[square]->piece;
    color_t color = piece_color(piece);
    piece_type_t type = piece_type(piece);
    // Swap the piece at the end of pos->pieces with the removed piece
    int end_index = pos->piece_count[color][type]--;
    pos->board[square]->piece = pos->pieces[color][type][end_index].piece;
    pos->board[square]->location = pos->pieces[color][type][end_index].location;
    pos->board[square] = NULL;
}

void transfer_piece(position_t* pos, const square_t from, const square_t to)
{
    if (pos->board[to]) {
        remove_piece(pos, to);
    }
    pos->board[to] = pos->board[from];
    pos->board[from] = NULL;
    pos->board[to]->location = to;
}

void do_move(position_t* position, const move_t move, undo_info_t* undo)
{
}

void undo_move(position_t* position, const move_t move, undo_info_t* undo)
{
}
