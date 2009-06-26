
#include "grasshopper.h"

const int sliding_piece_types[] = {
    0, 0, 0, DIAGONAL, STRAIGHT, BOTH, 0,
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

// Precomputed data for each (from,to) pair on what pieces can attack there.
// Originally computed in generate_attack_data().
const attack_data_t board_attack_data_storage[256] = {
  {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
  {  0,   0}, { 40,  17}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
  { 48,  16}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, { 40,  15},
  {  0,   0}, {  0,   0}, { 40,  17}, {  0,   0}, {  0,   0}, {  4,  33}, {  0,   0}, {  0,   0},
  { 48,  16}, {  0,   0}, {  0,   0}, {  4,  31}, {  0,   0}, {  0,   0}, { 40,  15}, {  0,   0},
  {  0,   0}, {  0,   0}, {  0,   0}, { 40,  17}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
  { 48,  16}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, { 40,  15}, {  0,   0}, {  0,   0},
  {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, { 40,  17}, {  0,   0}, {  4,  33}, {  0,   0},
  { 48,  16}, {  0,   0}, {  4,  31}, {  0,   0}, { 40,  15}, {  0,   0}, {  0,   0}, {  0,   0},
  {  0,   0}, {  0,   0}, {  4,  18}, {  0,   0}, {  0,   0}, { 40,  17}, {  0,   0}, {  0,   0},
  { 48,  16}, {  0,   0}, {  0,   0}, { 40,  15}, {  0,   0}, {  0,   0}, {  4,  14}, {  0,   0},
  {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  4,  18}, {  0,   0}, { 40,  17}, {  4,  33},
  { 48,  16}, {  4,  31}, { 40,  15}, {  0,   0}, {  4,  14}, {  0,   0}, {  0,   0}, {  0,   0},
  {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  4,  18}, {105,  17},
  {112,  16}, {105,  15}, {  4,  14}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
  {  0,   0}, { 48,   1}, { 48,   1}, { 48,   1}, { 48,   1}, { 48,   1}, { 48,   1}, {112,   1},
  {  0,   0}, {112,  -1}, { 48,  -1}, { 48,  -1}, { 48,  -1}, { 48,  -1}, { 48,  -1}, { 48,  -1},
  {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  4, -14}, {106, -15},
  {112, -16}, {106, -17}, {  4, -18}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
  {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  4, -14}, {  0,   0}, { 40, -15}, {  4, -31},
  { 48, -16}, {  4, -33}, { 40, -17}, {  0,   0}, {  4, -18}, {  0,   0}, {  0,   0}, {  0,   0},
  {  0,   0}, {  0,   0}, {  4, -14}, {  0,   0}, {  0,   0}, { 40, -15}, {  0,   0}, {  0,   0},
  { 48, -16}, {  0,   0}, {  0,   0}, { 40, -17}, {  0,   0}, {  0,   0}, {  4, -18}, {  0,   0},
  {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, { 40, -15}, {  0,   0}, {  4, -31}, {  0,   0},
  { 48, -16}, {  0,   0}, {  4, -33}, {  0,   0}, { 40, -17}, {  0,   0}, {  0,   0}, {  0,   0},
  {  0,   0}, {  0,   0}, {  0,   0}, { 40, -15}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
  { 48, -16}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, { 40, -17}, {  0,   0}, {  0,   0},
  {  0,   0}, {  0,   0}, { 40, -15}, {  0,   0}, {  0,   0}, {  4, -31}, {  0,   0}, {  0,   0},
  { 48, -16}, {  0,   0}, {  0,   0}, {  4, -33}, {  0,   0}, {  0,   0}, { 40, -17}, {  0,   0},
  {  0,   0}, { 40, -15}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
  { 48, -16}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, { 40, -17},
  {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
};
const attack_data_t* board_attack_data = board_attack_data_storage + 128;

/**
 * This is the function originally used to generate the static board attack data. It's not used
 * for anything now, but if the attack data structures are changed and this data needs to be
 * regenerated, this could come in handy.
 */
#include <strings.h>
#include <stdio.h>
void generate_attack_data(void)
{
    bzero((char*)board_attack_data_storage, sizeof(attack_data_t)*256);
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

    printf("{\n\t");
    for (int i=0; i<256; ++i) {
        printf("{%3d, %3d}, ",
                board_attack_data_storage[i].possible_attackers,
                board_attack_data_storage[i].relative_direction);
        if ((i+1)%8 == 0) printf("\n    ");
    }
    printf("}\n");
}
