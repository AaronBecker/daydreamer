
#ifndef MOVE_SELECTION_H
#define MOVE_SELECTION_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROOT_GENERATION, PV_GENERATION, NORMAL_GENERATION,
    ESCAPE_GENERATION, QUIESCENT_GENERATION, QUIESCENT_D0_GENERATION
} generation_t;

typedef struct {
    move_t moves[256];
    int scores[256];
    move_t bad_captures[256];
    int bad_scores[256];
    int current_move_index;
    int current_phase_index;
    phase_t current_phase;
    generation_t generator;
    move_t hash_move;
    move_t killers[4];
    int depth;
    position_t* pos;
} move_selector_t;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // SELECTION_H
