
#ifndef SEARCH_H
#define SEARCH_H
#ifdef __cplusplus
extern "C" {
#endif

#define PLY                 100
#define MAX_SEARCH_DEPTH    (127*PLY)
#define depth_to_index(x)   ((x)/PLY)

typedef enum {
    SEARCH_ABORTED, SEARCH_FAIL_HIGH, SEARCH_FAIL_LOW, SEARCH_EXACT
} search_result_t;

typedef struct {
    move_t pv[MAX_SEARCH_DEPTH+1];
    move_t killers[2];
    move_t mate_killer;
} search_node_t;

typedef enum {
    ENGINE_IDLE=0, ENGINE_PONDERING, ENGINE_THINKING, ENGINE_ABORTED
} engine_status_t;

typedef int score_type_t;
#define SCORE_LOWERBOUND    0x01
#define SCORE_UPPERBOUND    0x02
#define SCORE_EXACT         0x03
#define SCORE_MASK          0x03
#define MATE_THREAT         0x04

typedef struct {
    int multi_pv;
    int output_delay;
    bool use_book;
    bool use_egbb;
    bool verbose;
    bool chess960;
    bool arena_castle;
    bool ponder;
} options_t;

extern options_t options;

#define HIST_BUCKETS    15

typedef struct {
    int transposition_cutoffs[MAX_SEARCH_DEPTH + 1];
    int nullmove_cutoffs[MAX_SEARCH_DEPTH + 1];
    int move_selection[HIST_BUCKETS + 1];
    int pv_move_selection[HIST_BUCKETS + 1];
    int razor_attempts[3];
    int razor_prunes[3];
    int root_fail_highs;
    int root_fail_lows;
    int egbb_hits;
} search_stats_t;

typedef struct {
    int history[16*64]; // move indexed by piece type and destination square
    int success[16*64];
    int failure[16*64];
} history_t;

#define MAX_HISTORY         1000000
#define MAX_HISTORY_INDEX   (16*64)
#define depth_to_history(d) ((d)*(d) / (PLY*PLY))
#define history_index(m)   \
    ((get_move_piece_type(m)<<6)|(square_to_index(get_move_to(m))))

typedef struct {
    uint64_t nodes;
    move_t move;
    int score;
    int max_ply;
    int qsearch_score;
    move_t pv[MAX_SEARCH_DEPTH + 1];
} root_move_t;

typedef struct {
    position_t root_pos;
    search_stats_t stats;

    // search state info
    root_move_t root_moves[256];
    root_move_t* current_root_move;
    int best_score;
    int scores_by_iteration[MAX_SEARCH_DEPTH + 1];
    int root_indecisiveness;
    move_t pv[MAX_SEARCH_DEPTH + 1];
    search_node_t search_stack[MAX_SEARCH_DEPTH + 1];
    history_t history;
    uint64_t nodes_searched;
    uint64_t qnodes_searched;
    uint64_t pvnodes_searched;
    int current_depth;
    int current_move_index;
    bool resolving_fail_high;
    move_t obvious_move;
    engine_status_t engine_status;

    // when should we stop?
    milli_timer_t timer;
    uint64_t node_limit;
    int depth_limit;
    int time_limit;
    int time_target;
    int time_bonus;
    int mate_search; // TODO:implement
    bool infinite;
} search_data_t;

extern search_data_t root_data;

#define POLL_INTERVAL   0x3fff
#define MATE_VALUE      32000
#define DRAW_VALUE      0

#define is_mate_score(score)    \
    ((score+MAX_SEARCH_DEPTH+1>MATE_VALUE) || \
     (score-MAX_SEARCH_DEPTH-1<-MATE_VALUE))
#define mate_in(ply)                (MATE_VALUE-ply)
#define mated_in(ply)               (-MATE_VALUE+ply)
#define should_output(s)    \
    (elapsed_time(&((s)->timer)) > options.output_delay)


#ifdef __cplusplus
} // extern "C"
#endif
#endif // SEARCH_H
