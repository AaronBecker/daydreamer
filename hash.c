
#include "daydreamer.h"
#include "random.inc"

/*
 * Calculate the hash of a position from scratch. Used to in |set_position|,
 * and to verify the correctness of incremental hash updates in debug mode.
 */
hashkey_t hash_position(const position_t* pos)
{
    hashkey_t hash = 0;
    for (square_t sq=A1; sq<=H8; ++sq) {
        if (!valid_board_index(sq) || !pos->board[sq]) continue;
        hash ^= piece_hash(pos->board[sq]->piece, sq);
    }
    hash ^= ep_hash(pos);
    hash ^= castle_hash(pos);
    hash ^= side_hash(pos);
    return hash;
}

