
#include "daydreamer.h"

static const int mobility_score_table[2][8][32] = {
    { // midgame
        {0}, {0},
        {-8, -4, 0, 4, 8, 12, 16, 18, 20},
        {-15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 40, 40, 40, 40},
        {-10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20},
        {-20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7,
            -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
        {0}, {0}
    },
    { // endgame
        {0}, {0},
        {-8, -4, 0, 4, 8, 12, 16, 18, 20},
        {-15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 40, 40, 40, 40},
        {-10, -6, -2, 2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50},
        {-20, -18, -16, -14, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12,
            14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42},
        {0}, {0}
    },
};

static const int color_table[2][17] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0}, // white
    {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // black
};

// TODO: test and tune
int imbalance[9][9] = {
    {-126, -126, -126, -126, -126, -126, -126, -126,  -42 },
    {-126, -126, -126, -126, -126, -126, -126,  -42,   42 },
    {-126, -126, -126, -126, -126, -126,  -42,   42,   84 },
    {-126, -126, -126, -126, -104,  -42,   42,   84,  126 },
    {-126, -126, -126,  -88,    0,   88,  126,  126,  126 },
    {-126,  -84,  -42,   42,  104,  126,  126,  126,  126 },
    { -84,  -42,   42,  126,  126,  126,  126,  126,  126 },
    { -42,   42,  126,  126,  126,  126,  126,  126,  126 },
    {  42,  126,  126,  126,  126,  126,  126,  126,  126 }
};

static const int trapped_bishop = 150;
//static const int rook_on_7[2] = { 20, 40 };
static const int rook_on_7[2] = { 0, 0 };

/*
 * Compute the number of squares each non-pawn, non-king piece could move to,
 * and assign a bonus or penalty accordingly.
 */
score_t mobility_score(const position_t* pos)
{
    score_t score;
    int mid_score[2] = {0, 0};
    int end_score[2] = {0, 0};
    int majors[2] = {0, 0};
    int minors[2] = {0, 0};
    color_t side;
    for (side=WHITE; side<=BLACK; ++side) {
        const int* mobile = color_table[side];
        square_t from, to;
        piece_t piece;
        for (int i=1; (from = pos->pieces[side][i]) != INVALID_SQUARE; ++i) {
            piece = pos->board[from];
            piece_type_t type = piece_type(piece);
            if (type == PAWN || type == KING) continue;
            int ps = 0;
            switch (type) {
                case KNIGHT:
                    ps += mobile[pos->board[from-33]];
                    ps += mobile[pos->board[from-31]];
                    ps += mobile[pos->board[from-18]];
                    ps += mobile[pos->board[from-14]];
                    ps += mobile[pos->board[from+14]];
                    ps += mobile[pos->board[from+18]];
                    ps += mobile[pos->board[from+31]];
                    ps += mobile[pos->board[from+33]];
                    minors[side]++;
                    break;
                case QUEEN:
                    for (to=from-17; pos->board[to]==EMPTY; to-=17, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from-15; pos->board[to]==EMPTY; to-=15, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+15; pos->board[to]==EMPTY; to+=15, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+17; pos->board[to]==EMPTY; to+=17, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from-16; pos->board[to]==EMPTY; to-=16, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from-1; pos->board[to]==EMPTY; to-=1, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+1; pos->board[to]==EMPTY; to+=1, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+16; pos->board[to]==EMPTY; to+=16, ++ps) {}
                    ps += mobile[pos->board[to]];
                    majors[side] += 2;
                    break;
                case BISHOP:
                    for (to=from-17; pos->board[to]==EMPTY; to-=17, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from-15; pos->board[to]==EMPTY; to-=15, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+15; pos->board[to]==EMPTY; to+=15, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+17; pos->board[to]==EMPTY; to+=17, ++ps) {}
                    ps += mobile[pos->board[to]];
                    minors[side]++;
                    break;
                case ROOK:
                    for (to=from-16; pos->board[to]==EMPTY; to-=16, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from-1; pos->board[to]==EMPTY; to-=1, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+1; pos->board[to]==EMPTY; to+=1, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+16; pos->board[to]==EMPTY; to+=16, ++ps) {}
                    ps += mobile[pos->board[to]];
                    majors[side]++;
                    break;
                default: break;
            }
            mid_score[side] += mobility_score_table[0][type][ps];
            end_score[side] += mobility_score_table[1][type][ps];
        }
    }
    side = pos->side_to_move;
    score.midgame = mid_score[side] - mid_score[side^1];
    score.endgame = end_score[side] - end_score[side^1];
    int imb_score = imbalance
        [CLAMP(majors[side]-majors[side^1]+4, 0, 8)]
        [CLAMP(minors[side]-minors[side^1]+4, 0, 8)];
    score.midgame += imb_score;
    score.endgame += imb_score;
    return score;
}

