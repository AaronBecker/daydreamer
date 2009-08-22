
#include "daydreamer.h"

/*
 * Do some basic consistency checking on |pos| to identify bugs.
 */
void _check_board_validity(const position_t* pos)
{
    assert(pos->board[pos->pieces[WHITE][0]] == WK);
    assert(pos->board[pos->pieces[BLACK][0]] == BK);
    assert(pos->num_pawns[WHITE] <= 8);
    assert(pos->num_pawns[BLACK] <= 8);
    for (square_t sq=A1; sq<=H8; ++sq) {
        if (!valid_board_index(sq) || !pos->board[sq]) continue;
        piece_t piece = pos->board[sq];
        color_t side = piece_color(piece);
        if (piece_is_type(piece, PAWN)) {
            assert(pos->pawns[side][pos->piece_index[sq]] == sq);
        } else {
            assert(pos->pieces[side][pos->piece_index[sq]] == sq);
        }
    }
    for (int i=0; i<pos->num_pieces[0]; ++i) {
        assert(pos->piece_index[pos->pieces[0][i]] == i);
    }
    for (int i=0; i<pos->num_pawns[0]; ++i) {
        assert(pos->piece_index[pos->pawns[0][i]] == i);
    }
    for (int i=0; i<pos->num_pieces[1]; ++i) {
        assert(pos->piece_index[pos->pieces[1][i]] == i);
    }
    for (int i=0; i<pos->num_pawns[1]; ++i) {
        assert(pos->piece_index[pos->pawns[1][i]] == i);
    }
    assert(hash_position(pos) == pos->hash);
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
    (void)pos,(void)move;                           // Avoid warning when
    (void)from,(void)to,(void)piece,(void)capture;  // NDEBUG is defined.
    assert(valid_board_index(from) && valid_board_index(to));
    assert(pos->board[from] == piece);
    if (capture && !is_move_enpassant(move)) {
        assert(pos->board[to] == capture);
    } else {
        assert(pos->board[to] == EMPTY);
    }
}

/*
 * Check to see if |pos|'s incremental hash is correct.
 */
void _check_position_hash(const position_t* pos)
{
    (void)pos; // Avoid warning when NDEBUG is defined.
    assert(hash_position(pos) == pos->hash);
}

/*
 * Verify that a given list of moves in valid in the given position.
 */
void _check_line(position_t* pos, move_t* moves)
{
    if (moves[0] == NO_MOVE) return;
    undo_info_t undo;
    if (moves[0] == NULL_MOVE) do_nullmove(pos, &undo);
    else do_move(pos, moves[0], &undo);
    _check_line(pos, moves+1);
    if (moves[0] == NULL_MOVE) undo_nullmove(pos, &undo);
    else undo_move(pos, moves[0], &undo);
}

// TODO: _check_eval that makes sure white's eval is -1 * black's.

