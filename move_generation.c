
#include "grasshopper.h"

const uint32_t piece_deltas[16][16] = {
    // White Pieces
    {0},                                        // Null
    {15, 17, 0},                                // Pawn
    {-33, -31, -18, -14, 14, 18, 31, 33, 0},    // Knight
    {-17, -15, 15, 17},                         // Bishop
    {-16, -1, 1, 16, 0},                        // Rook
    {-17, -16, -15, -1, 1, 15, 16, 17},         // Queen
    {-17, -16, -15, -1, 1, 15, 16, 17},         // King
    {0}, {0},                                   // Null
    // Black Pieces
    {15, 17, 0},                                // Pawn
    {-33, -31, -18, -14, 14, 18, 31, 33, 0},    // Knight
    {-17, -15, 15, 17},                         // Bishop
    {-16, -1, 1, 16, 0},                        // Rook
    {-17, -16, -15, -1, 1, 15, 16, 17},         // Queen
    {-17, -16, -15, -1, 1, 15, 16, 17},         // King
    {0}                                         // Null
};

move_t* generate_moves(const position_t* pos, move_t* moves_begin)
{

}
