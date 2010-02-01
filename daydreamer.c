
#include "daydreamer.h"
#include <string.h>
#include <strings.h>

search_data_t root_data;
options_t options;

/*
 * Set up the stuff that only needs to be done once, during initialization.
 */
void init_daydreamer(void)
{
    init_hash();
    init_material_table(4*1024*1024);
    init_bitboards();
    generate_attack_data();
    init_eval();
    init_uci_options();
    set_position(&root_data.root_pos, FEN_STARTPOS);
}

const int material_values[] = {
    0, PAWN_VAL, KNIGHT_VAL, BISHOP_VAL, ROOK_VAL, QUEEN_VAL, KING_VAL, 0,
    0, PAWN_VAL, KNIGHT_VAL, BISHOP_VAL, ROOK_VAL, QUEEN_VAL, KING_VAL, 0, 0
};
const int eg_material_values[] = {
    0, EG_PAWN_VAL, EG_KNIGHT_VAL, EG_BISHOP_VAL,
    EG_ROOK_VAL, EG_QUEEN_VAL, EG_KING_VAL, 0,
    0, EG_PAWN_VAL, EG_KNIGHT_VAL, EG_BISHOP_VAL,
    EG_ROOK_VAL, EG_QUEEN_VAL, EG_KING_VAL, 0, 0
};


const char glyphs[] = ".PNBRQK  pnbrqk";

const slide_t sliding_piece_types[] = {
    0, 0, 0, DIAGONAL, STRAIGHT, BOTH, 0, 0,
    0, 0, 0, DIAGONAL, STRAIGHT, BOTH, 0, 0, 0
};

const direction_t piece_deltas[17][16] = {
    // White Pieces
    {0},                                                    // Null
    {NW, NE, 0},                                            // Pawn
    {SSW, SSE, WSW, ESE, WNW, ENE, NNW, NNE, 0},            // Knight
    {SW, SE, NW, NE, 0},                                    // Bishop
    {S, W, E, N, 0},                                        // Rook
    {SW, S, SE, W, E, NW, N, NE, 0},                        // Queen
    {SW, S, SE, W, E, NW, N, NE, 0},                        // King
    {0}, {0},                                               // Null
    // Black Pieces
    {SE, SW, 0},                                            // Pawn
    {SSW, SSE, WSW, ESE, WNW, ENE, NNW, NNE, 0},            // Knight
    {SW, SE, NW, NE, 0},                                    // Bishop
    {S, W, E, N, 0},                                        // Rook
    {SW, S, SE, W, E, NW, N, NE, 0},                        // Queen
    {SW, S, SE, W, E, NW, N, NE, 0},                        // King
    {0}, {0}                                                // Null
};

const direction_t pawn_push[] = {N, S};
const rank_t relative_rank[2][8] = {
    {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8},
    {RANK_8, RANK_7, RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, RANK_1}
};

const piece_flag_t piece_flags[] = {
    0, WP_FLAG, N_FLAG, B_FLAG, R_FLAG, Q_FLAG, K_FLAG, 0,
    0, BP_FLAG, N_FLAG, B_FLAG, R_FLAG, Q_FLAG, K_FLAG, 0, 0
};

square_t king_rook_home = H1;
square_t queen_rook_home = A1;
square_t king_home = E1;
