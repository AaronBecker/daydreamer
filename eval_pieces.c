
#include "daydreamer.h"

static const int mobility_score_table[2][8][32] = {
    { // midgame
        {0},
        {0, 4},
        {-24, -18, -12, -6, 0, 6, 12, 18, 24},
        {-35, -30, -25, -20, -15, -10, -5,  0, 5, 10, 15, 20, 25, 30, 35, 40},
        {-14, -12, -10,  -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, 14, 16},
        {-30, -28, -26, -24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -4, -2, 0,
            2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32},
        {0}, {0}
    },
    { // endgame
        {0},
        {0, 12},
        {-20, -15, -10, -5, 0, 10, 15, 20, 25},
        {-35, -30, -25, -20, -15, -10, -5,  0, 5, 10, 15, 20, 25, 30, 35, 40},
        {-35, -30, -25, -20, -15, -10, -5,  0, 5, 10, 15, 20, 25, 30, 35, 40},
        {-45, -42, -39, -36, -33, -30, -27, -24, -21, -18, -15, -12, -9, -6, -3, 0, 3,
            6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48},
        {0}, {0}
    },
};

static const int color_table[2][17] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0}, // white
    {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // black
};

static const int knight_outpost[0x80] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  2,  3,  3,  3,  3,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  2,  3,  3,  3,  3,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  1,  2,  3,  3,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};
static const int bishop_outpost[0x80] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    2,  3,  3,  3,  3,  3,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    2,  3,  3,  3,  3,  3,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

// TODO: these really need to be tuned
static const int rook_on_7[2] = { 5, 15 };
static const int rook_open_file_bonus[2] = { 13, 11 };
static const int rook_half_open_file_bonus[2] = { 5, 7 };

/*
 * Score a weak square that's occupied by a minor piece. The basic bonus
 * is given by a score table, and additional points are awarded for being
 * defended by a friendly pawn and for being difficult to take with an
 * opponent's minor piece.
 */
static int outpost_score(const position_t* pos, square_t sq, piece_type_t type)
{
    int bonus = type == KNIGHT ? knight_outpost[sq] : bishop_outpost[sq];
    int score = bonus;
    if (bonus) {
        // An outpost is better when supported by pawns.
        color_t side = piece_color(pos->board[sq]);
        piece_t our_pawn = create_piece(side, PAWN);
        if (pos->board[sq - pawn_push[side] - 1] == our_pawn ||
                pos->board[sq - pawn_push[side] + 1] == our_pawn) {
            score += bonus/2;
            // Even better if an opposing knight/bishop can't capture it.
            // TODO: take care of the case where there's one opposing bishop
            // that's the wrong color. The position data structure needs to
            // be modified a little to make this efficient.
            piece_t their_knight = create_piece(side^1, KNIGHT);
            piece_t their_bishop = create_piece(side^1, BISHOP);
            if (pos->piece_count[their_knight] == 0 &&
                    pos->piece_count[their_bishop] == 0) {
                score += bonus;
            }
        }
    }
    return score;
}

/*
 * Compute the number of squares each non-pawn, non-king piece could move to,
 * and assign a bonus or penalty accordingly.
 */
score_t pieces_score(const position_t* pos, pawn_data_t* pd)
{
    score_t score;
    int mid_score[2] = {0, 0};
    int end_score[2] = {0, 0};
    rank_t king_rank[2] = { relative_rank[WHITE]
                                [square_rank(pos->pieces[WHITE][0])],
                            relative_rank[BLACK]
                                [square_rank(pos->pieces[BLACK][0])] };
    color_t side;
    for (side=WHITE; side<=BLACK; ++side) {
        const int* mobile = color_table[side];
        square_t from, to;
        piece_t piece, dummy;
        int push = pawn_push[side];
        for (int i=1; pos->pieces[side][i] != INVALID_SQUARE; ++i) {
            from = pos->pieces[side][i];
            piece = pos->board[from];
            piece_type_t type = piece_type(piece);
            int ps = 0;
            switch (type) {
                case PAWN:
                    ps = (pos->board[from+push] == EMPTY);
                    dummy = pos->board[from+push-1];
                    if (dummy > create_piece(side^1, PAWN) &&
                            dummy <= create_piece(side^1, KING)) {
                        mid_score[side] += 7;
                        end_score[side] += 12;
                    }
                    dummy = pos->board[from+push+1];
                    if (dummy > create_piece(side^1, PAWN) &&
                            dummy <= create_piece(side^1, KING)) {
                        mid_score[side] += 7;
                        end_score[side] += 12;
                    }
                    break;
                case KNIGHT:
                    ps += mobile[pos->board[from-33]];
                    ps += mobile[pos->board[from-31]];
                    ps += mobile[pos->board[from-18]];
                    ps += mobile[pos->board[from-14]];
                    ps += mobile[pos->board[from+14]];
                    ps += mobile[pos->board[from+18]];
                    ps += mobile[pos->board[from+31]];
                    ps += mobile[pos->board[from+33]];
                    if (square_is_outpost(pd, from, side)) {
                        int bonus = outpost_score(pos, from, KNIGHT);
                        mid_score[side] += bonus;
                        end_score[side] += bonus;
                    }
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
                    if (relative_rank[side][square_rank(from)] == RANK_7 &&
                            king_rank[side^1] == RANK_8) {
                        mid_score[side] += rook_on_7[0] / 2;
                        end_score[side] += rook_on_7[1] / 2;
                    }
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
                    if (square_is_outpost(pd, from, side)) {
                        int bonus = outpost_score(pos, from, BISHOP);
                        mid_score[side] += bonus;
                        end_score[side] += bonus;
                    }
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
                    int rrank = relative_rank[side][square_rank(from)];
                    if (rrank == RANK_7 && king_rank[side^1] == RANK_8) {
                        mid_score[side] += rook_on_7[0];
                        end_score[side] += rook_on_7[1];
                    }
                    file_t file = square_file(from);
                    if (file_is_half_open(pd, file, side)) {
                        mid_score[side] += rook_half_open_file_bonus[0];
                        end_score[side] += rook_half_open_file_bonus[1];
                        if (file_is_half_open(pd, file, side^1)) {
                            mid_score[side] += rook_open_file_bonus[0];
                            end_score[side] += rook_open_file_bonus[1];
                        }
                    }
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
    return score;
}

