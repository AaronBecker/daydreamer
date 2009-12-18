
#ifndef MOVE_SELECTION_H
#define MOVE_SELECTION_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROOT_GEN, PV_GEN, NONPV_GEN,
    ESCAPE_GEN, Q_GEN, Q_CHECK_GEN,
} generation_t;

typedef struct {
    move_t moves[256];
    int scores[256];
    int moves_end;
    int current_move_index;
    generation_t generator;
    move_t hash_move;
    move_t mate_killer;
    move_t killers[5];
    int moves_so_far;
    int ordered_moves;
    int depth;
    position_t* pos;
    bool single_reply;
} move_selector_t;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // SELECTION_H
