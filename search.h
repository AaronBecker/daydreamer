
#ifndef SEARCH_H
#define SEARCH_H
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SEARCH_DEPTH        64

typedef struct {
    move_t pv[MAX_SEARCH_DEPTH];
} search_node_t;

typedef enum {
    ENGINE_IDLE=0, ENGINE_PONDERING, ENGINE_THINKING, ENGINE_ABORTED
} engine_status_t;

typedef enum {
    SCORE_EXACT, SCORE_LOWERBOUND, SCORE_UPPERBOUND
} score_type_t;

typedef struct {
    int poll_interval;
    int output_delay;
} search_options_t;

typedef struct {
    position_t root_pos;
    search_options_t options;

    // search state info
    move_t root_moves[256];
    move_t best_move; // FIXME: shouldn't this be redundant with pv[0]?
    int best_score;
    move_t pv[MAX_SEARCH_DEPTH];
    search_node_t search_stack[MAX_SEARCH_DEPTH];
    uint64_t nodes_searched;
    int current_depth;
    engine_status_t engine_status;

    // when should we stop?
    milli_timer_t timer;
    uint64_t node_limit;
    int depth_limit;
    int time_limit;
    int time_target;
    int mate_search;
    bool infinite;
    bool ponder;
} search_data_t;

#define POLL_INTERVAL   0xffff
#define MATE_VALUE      0x7fff
#define DRAW_VALUE      0
#define NULL_R          3
#define NULLMOVE_DEPTH_REDUCTION    4

#define is_mate_score(score)       (abs(score) + 256 > MATE_VALUE)
#define should_output(s)    \
    (elapsed_time(&((s)->timer)) > (s)->options.output_delay)


#ifdef __cplusplus
} // extern "C"
#endif
#endif // SEARCH_H
