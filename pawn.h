
#ifndef PAWN_H
#define PAWN_H
#ifdef __cplusplus
extern "C" {
#endif
    
typedef struct {
    bitboard_t pawns_bb[2];
    bitboard_t outposts_bb[2];
    bitboard_t passed_bb[2];
    square_t passed[2][8];
    int num_passed[2];
    score_t score[2];
    int kingside_storm[2];
    int queenside_storm[2];
    hashkey_t key;
} pawn_data_t;

#define square_is_outpost(pd, sq, side) \
    (sq_bit_is_set(pd->outposts_bb[side], sq))
#define file_is_half_open(pd, file, side) \
    (((pd)->pawns_bb[side] & file_mask[file]) == EMPTY_BB)

#ifdef __cplusplus
}
#endif
#endif
