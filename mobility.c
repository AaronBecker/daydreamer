
#include "daydreamer.h"

int base_mobility_score[2][8] = {
    {0, 0, -4, -6, -7, -13, 0}, //midgame
    {0, 0, -4, -6, -7, -13, 0}, //endgame
};

int mobility_multiplier[2][8] = {
    {0, 0, 4, 5, 2, 1, 0}, // midgame
    {0, 0, 4, 5, 4, 2, 0}, // endgame
};

int color_table[2][17] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0}, // white
    {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // black
};

void mobility_score(const position_t* pos, score_t* score)
{
    for (color_t side=WHITE; side<=BLACK; ++side) {
        int* mobile = color_table[side];
        square_t from, to;
        piece_t piece;
        for (square_t* pfrom = &((position_t*)pos)->pieces[side][1];
                (from = *pfrom) != INVALID_SQUARE;
                ++pfrom) {
            piece = pos->board[from];
            piece_type_t type = piece_type(piece);
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
                    break;
                case QUEEN:
                    for (to=from-17; pos->board[to]==EMPTY; to-=17, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from-16; pos->board[to]==EMPTY; to-=16, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from-15; pos->board[to]==EMPTY; to-=15, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from-1; pos->board[to]==EMPTY; to-=1, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+1; pos->board[to]==EMPTY; to+=1, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+15; pos->board[to]==EMPTY; to+=15, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+16; pos->board[to]==EMPTY; to+=16, ++ps) {}
                    ps += mobile[pos->board[to]];
                    for (to=from+17; pos->board[to]==EMPTY; to+=17, ++ps) {}
                    ps += mobile[pos->board[to]];
                default:
                    break;
            }
            score->midgame += (base_mobility_score[0][type] +
                    ps*mobility_multiplier[0][type]) *
                (side == pos->side_to_move ? 1 : -1);
            score->endgame += (base_mobility_score[1][type] +
                    ps*mobility_multiplier[1][type]) *
                (side == pos->side_to_move ? 1 : -1);
        }
    }
}

