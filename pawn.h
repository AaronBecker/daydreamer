
#ifndef PAWN_H
#define PAWN_H
#ifdef __cplusplus
extern "C" {
#endif
    
typedef struct {
    square_t passed[2][8];
    int num_passed[2];
    int score[2];
    int endgame_score[2];
    hashkey_t key;
} pawn_data_t;

#ifdef __cplusplus
}
#endif
#endif
