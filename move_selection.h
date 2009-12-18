
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
    
    GENERIC_MOVES,
    GENERIC_QSEARCH,
    GENERIC_QSEARCH_CH,

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
    move_t* bad_tactic_tail;
    move_t* current_move_list;
    selection_phase_t* phase;
    int moves_end_index;
    int current_move_index;
    generation_t generator;
    move_t hash_move[2];
    move_t mate_killer;
    move_t killers[5];
    int moves_so_far;
    int ordered_moves;
    int depth;
    position_t* pos;
} move_selector_t;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // SELECTION_H
