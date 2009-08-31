
#ifndef MOVE_SELECTION_H
#define MOVE_SELECTION_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROOT_GENERATION, PV_GENERATION, NORMAL_GENERATION,
    ESCAPE_GENERATION, QUIESCENT_GENERATION, QUIESCENT_D0_GENERATION
} generation_t;

typedef enum {
    PHASE_ROOT, PHASE_TRANS, PHASE_GOOD_TACTICS,
    PHASE_CHECKS, PHASE_BAD_TACTICS, PHASE_QUIET,
    PHASE_EVASIONS, PHASE_END
} phase_t;

typedef struct {
    move_t moves[256];
    int scores[256];
    move_t bad_tactics[256];
    int moves_end;
    int bad_end;
    int current_move_index;
    int phase_index;
    phase_t phase;
    generation_t generator;
    move_t hash_move;
    move_t killers[5];
    int moves_so_far;
    int depth;
    position_t* pos;
} move_selector_t;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // SELECTION_H
