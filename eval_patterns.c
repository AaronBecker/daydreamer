
#include "daydreamer.h"

static const int trapped_bishop = 100;

/*
 * Find simple bad patterns that won't show up within reasonable search
 * depths. This is mostly trapped and blocked pieces.
 */
score_t pattern_score(const position_t*pos)
{
    int s = 0;
    if ((pos->board[A2] == BB && pos->board[B3] == WP) ||
            (pos->board[B1] == BB && pos->board[C2] == WP) ||
            (pos->board[H2] == BB && pos->board[G3] == WP) ||
            (pos->board[G1] == BB && pos->board[F2] == WP)) {
        s += trapped_bishop;
    }
    if ((pos->board[A3] == BB && pos->board[B4] == WP) ||
            (pos->board[H3] == BB && pos->board[G4] == WP)) {
        s += trapped_bishop/2;
    }

    if ((pos->board[A7] == WB && pos->board[B6] == BP) ||
            (pos->board[B8] == WB && pos->board[C7] == BP) ||
            (pos->board[H7] == WB && pos->board[G6] == BP) ||
            (pos->board[G8] == WB && pos->board[F7] == BP)) {
        s -= trapped_bishop;
    }
    if ((pos->board[A6] == WB && pos->board[B5] == BP) ||
            (pos->board[H6] == WB && pos->board[G5] == BP)) {
        s -= trapped_bishop/2;
    }

    if (pos->side_to_move == BLACK) s *= -1;

    score_t score;
    score.midgame = s;
    score.endgame = s;
    return score;
}
