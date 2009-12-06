
#ifndef MOVE_SELECTION_H
#define MOVE_SELECTION_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROOT_GEN,
    PV_GEN,
    NONPV_GEN,
    ESCAPE_GEN,
    Q_GEN,
    Q_CHECK_GEN,
} generation_t;

typedef enum {
    PHASE_BEGIN,
    PHASE_END,
    ROOT_MOVES,
    TRANS_MOVE,
    KILLER_MOVES,
    GOOD_TACTICS,
    BAD_TACTICS,
    QUIET_MOVES,
    EVASIONS,
    QSEARCH_TACTICS,
    QSEARCH_CHECKS
} selection_phase_t;

typedef struct {
    move_t moves[256];
    int scores[256];
    move_t bad_tactics[64];
    int bad_tactic_scores[64];
    move_t* current_move;
    move_t* bad_tactic_tail;
    generation_t generator;
    selection_phase_t* phase;
    move_t hash_move[2];
    move_t killers[5];
    int depth;
    int moves_so_far;
    position_t* pos;
} move_selector_t;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // SELECTION_H
