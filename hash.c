
#include "daydreamer.h"
#include <string.h>

const hashkey_t piece_random[2][7][64];
const hashkey_t castle_random[2][2][2];
const hashkey_t enpassant_random[64];

static hashkey_t random_hashkey(void)
{
    hashkey_t hash = random();
    hash <<= 32;
    hash |= random();
    return hash;
}

void init_hash(void)
{
    int i;
    srandom(1);
    hashkey_t* _piece_random = (hashkey_t*)&piece_random[0][0][0];
    hashkey_t* _castle_random = (hashkey_t*)&castle_random[0][0][0];
    hashkey_t* _enpassant_random = (hashkey_t*)&enpassant_random[0];

    for (i=0; i<2*7*64; ++i) _piece_random[i] = random_hashkey();
    for (i=0; i<64; ++i) _enpassant_random[i] = random_hashkey();
    for (i=0; i<2*2*2; ++i) _castle_random[i] = random_hashkey();
}

/*
 * Calculate the hash of a position from scratch. Used to in |set_position|,
 * and to verify the correctness of incremental hash updates in debug mode.
 */
hashkey_t hash_position(const position_t* pos)
{
    hashkey_t hash = 0;
    for (square_t sq=A1; sq<=H8; ++sq) {
        if (!valid_board_index(sq) || !pos->board[sq]) continue;
        hash ^= piece_hash(pos->board[sq], sq);
    }
    hash ^= ep_hash(pos);
    hash ^= castle_hash(pos);
    hash ^= side_hash(pos);
    return hash;
}

hashkey_t hash_pawns(const position_t* pos)
{
    hashkey_t hash = 0;
    for (square_t sq=A1; sq<=H8; ++sq) {
        if (!valid_board_index(sq) || !pos->board[sq]) continue;
        if (!piece_is_type(pos->board[sq], PAWN)) continue;
        hash ^= piece_hash(pos->board[sq], sq);
    }
    return hash;
}
