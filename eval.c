
#include "daydreamer.h"

/* 
 * From The Evaluation of Material Imbalances, Larry Kaufman 1999.
 * home.comcast.net/~danheisman/Articles/evaluation_of_material_imbalance.htm
 */
#define PAWN_VAL      100
#define KNIGHT_VAL    325
#define BISHOP_VAL    325
#define ROOK_VAL      500
#define QUEEN_VAL     975
#define KING_VAL      10000

const int material_values[] = {
    0, PAWN_VAL, KNIGHT_VAL, BISHOP_VAL, ROOK_VAL, QUEEN_VAL, KING_VAL, 0,
    0, PAWN_VAL, KNIGHT_VAL, BISHOP_VAL, ROOK_VAL, QUEEN_VAL, KING_VAL, 0, 0
};

/*
 * Static scores that a piece gets for being on a particular square. These
 * encourage promoting pawns, centralizing knights, etc.
 * The values are taken directly from Scorpio.
 */
int piece_square_values[BK+1][0x80] = {

    {// empty (needed to get indexing right)
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },

    { // white pawn
      0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
      1,  2,  3,  5,  5,  3,  2,  1, 0, 0, 0, 0, 0, 0, 0, 0,
      2,  5,  8, 17, 17,  8,  5,  2, 0, 0, 0, 0, 0, 0, 0, 0,
      3,  7, 10, 26, 26, 10,  7,  3, 0, 0, 0, 0, 0, 0, 0, 0,
      6, 10, 15, 30, 30, 15, 10,  6, 0, 0, 0, 0, 0, 0, 0, 0,
     10, 14, 19, 34, 34, 19, 14, 10, 0, 0, 0, 0, 0, 0, 0, 0,
     10, 14, 19, 34, 34, 19, 14, 10, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0 },

    { // white knight
    -10, -10, -10, -10, -10, -10, -10, -10,  0,  0,  0,  0,  0,  0,  0,  0,  
    -10,  -5,  -5,  -5,  -5,  -5,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  -5,   0,   0,   0,   0,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  -5,   0,   5,   5,   0,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  -5,   0,   5,   5,   0,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  -5,   0,   0,   0,   0,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -20, -10, -10, -10, -10, -10, -10, -20,  0,  0,  0,  0,  0,  0,  0,  0,
    -20, -20, -20, -20, -20, -20, -20, -20,  0,  0,  0,  0,  0,  0,  0,  0 },

    { // white bishop
    -10, -10, -10, -10, -10, -10, -10, -10,  0,  0,  0,  0,  0,  0,  0,  0,  
    -10,  -5,  -5,  -5,  -5,  -5,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  -5,   0,   0,   0,   0,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  -5,   0,   5,   5,   0,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  -5,   0,   5,   5,   0,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  -5,   0,   0,   0,   0,  -5, -10,  0,  0,  0,  0,  0,  0,  0,  0,
    -20, -10, -10, -10, -10, -10, -10, -20,  0,  0,  0,  0,  0,  0,  0,  0,
    -20, -20, -20, -20, -20, -20, -20, -20,  0,  0,  0,  0,  0,  0,  0,  0 },

    { // white rook
    -6,  -3,  0,  3,  3,  0,  -3, -6,  0,  0,  0,  0,  0,  0,  0,  0,  
    -6,  -3,  0,  3,  3,  0,  -3, -6,  0,  0,  0,  0,  0,  0,  0,  0,  
    -6,  -3,  0,  3,  3,  0,  -3, -6,  0,  0,  0,  0,  0,  0,  0,  0,  
    -6,  -3,  0,  3,  3,  0,  -3, -6,  0,  0,  0,  0,  0,  0,  0,  0,  
    -6,  -3,  0,  3,  3,  0,  -3, -6,  0,  0,  0,  0,  0,  0,  0,  0,  
    -6,  -3,  0,  3,  3,  0,  -3, -6,  0,  0,  0,  0,  0,  0,  0,  0,  
    -6,  -3,  0,  3,  3,  0,  -3, -6,  0,  0,  0,  0,  0,  0,  0,  0,  
    -6,  -3,  0,  3,  3,  0,  -3, -6,  0,  0,  0,  0,  0,  0,  0,  0 },

    { // white queen
    -5,   -5,  -5,  -5,  -5,  -5,  -5,  -5,  0,  0,  0,  0,  0,  0,  0,  0,
    -5,    0,   0,   0,   0,   0,   0,  -5,  0,  0,  0,  0,  0,  0,  0,  0,
    -5,    0,   4,   4,   4,   4,   0,  -5,  0,  0,  0,  0,  0,  0,  0,  0,
    -5,    0,   4,   5,   5,   4,   0,  -5,  0,  0,  0,  0,  0,  0,  0,  0,
    -5,    0,   4,   5,   5,   4,   0,  -5,  0,  0,  0,  0,  0,  0,  0,  0,
    -5,    0,   4,   4,   4,   4,   0,  -5,  0,  0,  0,  0,  0,  0,  0,  0,
    -20,  -5,  -5,  -5,  -5,  -5,  -5, -20,  0,  0,  0,  0,  0,  0,  0,  0,
    -20, -20, -10, -10, -10, -10, -20, -20,  0,  0,  0,  0,  0,  0,  0,  0 },

    { // white king
    -20,   0, -20, -20, -20, -20,   0, -20,  0,  0,  0,  0,  0,  0,  0,  0,
    -20, -20, -20, -20, -20, -20, -20, -20,  0,  0,  0,  0,  0,  0,  0,  0,
    -30, -30, -30, -30, -30, -30, -30, -30,  0,  0,  0,  0,  0,  0,  0,  0,
    -40, -40, -40, -40, -40, -40, -40, -40,  0,  0,  0,  0,  0,  0,  0,  0,
    -50, -50, -50, -50, -50, -50, -50, -50,  0,  0,  0,  0,  0,  0,  0,  0,
    -50, -50, -50, -50, -50, -50, -50, -50,  0,  0,  0,  0,  0,  0,  0,  0,
    -50, -50, -50, -50, -50, -50, -50, -50,  0,  0,  0,  0,  0,  0,  0,  0,
    -50, -50, -50, -50, -50, -50, -50, -50,  0,  0,  0,  0,  0,  0,  0,  0 }

    // black piece tables are mirror images filled in during init_eval
};

/*
 * Initialize all static evaluation data structures.
 */
void init_eval(void)
{
    for (piece_t piece=BP; piece<=BK; ++piece) {
        for (square_t square=A1; square<=H8; ++square) {
            if (!valid_board_index(square)) continue;
            piece_square_values[piece][square] =
                piece_square_values[piece-BP+1][flip_square(square)];
        }
    }
}

/*
 * Perform a simple position evaluation based just on material and piece
 * square bonuses.
 */
int simple_eval(const position_t* pos)
{
    color_t side = pos->side_to_move;
    return pos->material_eval[side] - pos->material_eval[side^1] +
        pos->piece_square_eval[side] - pos->piece_square_eval[side^1];
}

bool insufficient_material(const position_t* pos)
{
    return (pos->piece_count[WHITE][PAWN] == 0 &&
        pos->piece_count[BLACK][PAWN] == 0 &&
        pos->material_eval[WHITE] < ROOK_VAL &&
        pos->material_eval[BLACK] < ROOK_VAL);
}

bool is_draw(const position_t* pos)
{
    return pos->fifty_move_counter >= 100 || insufficient_material(pos);
}
