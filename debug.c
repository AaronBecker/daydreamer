
#include <assert.h>
#include "daydreamer.h"

/*
 * Do some basic consistency checking on |pos| to identify bugs.
 */
void _check_board_validity(const position_t* pos)
{
    assert(pos->piece_count[0][KING] == 1);
    assert(pos->piece_count[1][KING] == 1);
    assert(pos->piece_count[0][PAWN] <= 8);
    assert(pos->piece_count[1][PAWN] <= 8);
    for (square_t sq=A1; sq<=H8; ++sq) {
        if (!valid_board_index(sq)) continue;
        if (pos->board[sq]) assert(pos->board[sq]->location == sq);
    }
    for (piece_type_t type=PAWN; type<=KING; ++type) {
        for (int i=0; i<pos->piece_count[0][type]; ++i)
            assert(pos->pieces[0][type][i].location == INVALID_SQUARE ||
                    pos->board[pos->pieces[0][type][i].location] ==
                    &pos->pieces[0][type][i]);
        for (int i=0; i<pos->piece_count[1][type]; ++i)
            assert(pos->pieces[1][type][i].location == INVALID_SQUARE ||
                    pos->board[pos->pieces[1][type][i].location] ==
                    &pos->pieces[1][type][i]);
    }
}

/*
 * Perform some sanity checks on |move| to flag obviously invalid moves.
 */
void _check_move_validity(const position_t* pos, const move_t move)
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
