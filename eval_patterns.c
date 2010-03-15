
#include "daydreamer.h"

static const int trapped_bishop = 150;

/*
 * Find simple bad patterns that won't show up within reasonable search
 * depths. This is mostly trapped and blocked pieces.
 * TODO: add some trapped knight/rook patterns here.
 * TODO: king luft?
 * TODO: maybe merge this with eval_pieces so we have access to piece
 * mobility information.
 */
score_t pattern_score(const position_t*pos)
{
    int s = 0;
    if (pos->board[A2] == BB && pos->board[B3] == WP) s += trapped_bishop;
    if (pos->board[B1] == BB && pos->board[C2] == WP) s += trapped_bishop;
    if (pos->board[H2] == BB && pos->board[G3] == WP) s += trapped_bishop;
    if (pos->board[G1] == BB && pos->board[F2] == WP) s += trapped_bishop;

    if (pos->board[A7] == WB && pos->board[B6] == BP) s -= trapped_bishop;
    if (pos->board[B8] == WB && pos->board[C7] == BP) s -= trapped_bishop;
    if (pos->board[H7] == WB && pos->board[G6] == BP) s -= trapped_bishop;
    if (pos->board[G8] == WB && pos->board[F7] == BP) s -= trapped_bishop;
    if (pos->side_to_move == BLACK) s *= -1;

    score_t score;
    score.midgame = s;
    score.endgame = s;
    return score;
}
