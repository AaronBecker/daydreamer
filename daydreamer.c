
#include "daydreamer.h"
#include <string.h>
#include <strings.h>

extern void init_eval(void);
extern search_data_t root_data;

/*
 * Set up the stuff that only needs to be done once, during initialization.
 */
void init_daydreamer(void)
{
    generate_attack_data();
    init_eval();
    init_transposition_table(64 * 1<<20);
    init_pawn_table(1 * 1<<20);
    init_uci_options();
    set_position(&root_data.root_pos, FEN_STARTPOS);
}

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

