
#include "grasshopper.h"
#include <assert.h>

void place_piece(position_t* pos, piece_t piece, square_t square)
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

void remove_piece(position_t* pos, square_t square)
{
    piece_t piece = pos->board[square]->piece;
    color_t color = piece_color(piece);
    piece_type_t type = piece_type(piece);
    int end_index = pos->piece_count[color][type]--;
    pos->board[square]->piece = pos->pieces[color][type][end_index].piece;
    pos->board[square]->location = pos->pieces[color][type][end_index].location;
    pos->board[square] = NULL;
}

void transfer_piece(position_t* pos, square_t from, square_t to)
{
    if (pos->board[to]) {
        remove_piece(pos, to);
    }
    pos->board[to] = pos->board[from];
    pos->board[from] = NULL;
    pos->board[to]->location = to;
}
