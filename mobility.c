
#include "daydreamer.h"

int base_mobility_score[2][8] = {
    {0, 0, -4, -6, -7, -13, 0}, //midgame
    {0, 0, -4, -6, -7, -13, 0}, //endgame
};

int mobility_multiplier[2][8] = {
    {0, 0, 4, 5, 2, 1, 0}, // midgame
    {0, 0, 4, 5, 4, 2, 0}, // endgame
};

int color_table[2][16] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // white
    {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // black
};

int mobility_score(const position_t* pos)
{
    int phase = 0; // TODO: endgame/midgame phase calculation
    int score = 0;
    for (color_t side=WHITE; side<=BLACK; ++side) {
        int* mobile = color_table[side];
        square_t from, to;
        piece_t piece;
        for (square_t* pfrom = &((position_t*)pos)->pieces[side][1];
                (from = *pfrom) != INVALID_SQUARE;
                ++pfrom) {
            piece = pos->board[from];
            piece_type_t type = piece_type(piece);
            int ps = base_mobility_score[phase][type];
            // TODO: a padded board representation would speed this up.
            switch (type) {
                case KNIGHT:
                    to = from-33;
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    to = from-31;
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    to = from-18;
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    to = from-14;
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    to = from+14;
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    to = from+18;
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    to = from+31;
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    to = from+33;
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    break;
                case BISHOP:
                    for (to=from-17; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to-=17, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from-15; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to-=15, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from+15; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to+=15, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from+17; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to+=17, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    break;
                case ROOK:
                    for (to=from-16; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to-=16, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from-1; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to-=1, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from+1; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to+=1, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from+16; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to+=16, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    break;
                case QUEEN:
                    for (to=from-17; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to-=17, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from-16; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to-=16, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from-15; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to-=15, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from-1; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to-=1, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from+1; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to+=1, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from+15; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to+=15, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from+16; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to+=16, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                    for (to=from+17; valid_board_index(to) &&
                            pos->board[to]==EMPTY; to+=17, ++ps) {}
                    if (valid_board_index(to)) ps += mobile[pos->board[to]];
                default:
                    break;
            }
            ps *= mobility_multiplier[phase][type];
            score += ps;
        }
        score *= -1;
    }
    if (pos->side_to_move == BLACK) score *= -1;
    return score;
}

