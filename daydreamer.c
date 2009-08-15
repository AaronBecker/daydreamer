
#include "daydreamer.h"
#include <string.h>
#include <strings.h>

extern void init_eval(void);
extern search_data_t root_data;
static void generate_attack_data(void);

/*
 * Set up the stuff that only needs to be done once, during initialization.
 */
void init_daydreamer(void)
{
    generate_attack_data();
    init_eval();
    init_transposition_table(64 * 1<<20);
    init_uci_options(&root_data.options);
}

const char glyphs[] = ".PNBRQK  pnbrqk";

const slide_t sliding_piece_types[] = {
    0, 0, 0, DIAGONAL, STRAIGHT, BOTH, 0, 0,
    0, 0, 0, DIAGONAL, STRAIGHT, BOTH, 0
};

const direction_t piece_deltas[16][16] = {
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
    {0}                                                     // Null
};

const direction_t pawn_push[] = {N, S};
const rank_t relative_pawn_rank[2][8] = {
    {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8},
    {RANK_8, RANK_7, RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, RANK_1}
};

const piece_flag_t piece_flags[] = {
    0, WP_FLAG, N_FLAG, B_FLAG, R_FLAG, Q_FLAG, K_FLAG, 0,
    0, BP_FLAG, N_FLAG, B_FLAG, R_FLAG, Q_FLAG, K_FLAG, 0, 0
};

// Data for each (from,to) pair on what pieces can attack there.
// Computed in generate_attack_data().
const attack_data_t board_attack_data_storage[256];
const attack_data_t* board_attack_data = board_attack_data_storage + 128;

/**
 * Calculate which pieces can attack from a given square to another square
 * for each possible (from,to) pair.
 */
static void generate_attack_data(void)
{
    memset((char*)board_attack_data_storage, 0, sizeof(attack_data_t)*256);
    attack_data_t* mutable_attack_data = (attack_data_t*)board_attack_data;
    for (square_t from=A1; from<=H8; ++from) {
        if (!valid_board_index(from)) continue;
        for (piece_t piece=WP; piece<=BK; ++piece) {
            for (const direction_t* dir=piece_deltas[piece]; *dir; ++dir) {
                for (square_t to=from+*dir; valid_board_index(to); to+=*dir) {
                    mutable_attack_data[from-to].possible_attackers |=
                        get_piece_flag(piece);
                    mutable_attack_data[from-to].relative_direction = *dir;
                    if (piece_slide_type(piece) == NO_SLIDE) break;
                }
            }
        }
    }
}
