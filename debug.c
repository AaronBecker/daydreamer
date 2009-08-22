
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
        if (!valid_board_index(sq) || !pos->board[sq]) continue;
        piece_t piece = pos->board[sq];
        color_t side = piece_color(piece);
        piece_type_t type = piece_type(piece);
        assert(pos->pieces[side][type][pos->piece_index[sq]] == sq);
    }
    for (piece_type_t type=PAWN; type<=KING; ++type) {
        for (int i=0; i<pos->piece_count[0][type]; ++i) {
            piece_t piece = create_piece(0, type);
            assert(pos->board[pos->pieces[0][type][i]] == piece);
        }
        for (int i=0; i<pos->piece_count[1][type]; ++i) {
            piece_t piece = create_piece(1, type);
            assert(pos->board[pos->pieces[1][type][i]] == piece);
        }
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

