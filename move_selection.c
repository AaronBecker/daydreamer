
#include "daydreamer.h"

extern search_data_t root_data;

static const int ordered_pv_moves = 256;
static const int ordered_nonpv_moves = 16;
static const int ordered_nonpv_moves = 16;
static const int ordered_quiescent_moves = 4;

typedef enum {
    PHASE_ROOT, PHASE_TRANS, PHASE_KILLERS, PHASE_GOOD_CAPS,
    PHASE_CHECKS, PHASE_BAD_CAPS, PHASE_QUIET, PHASE_PROMOTES,
    PHASE_UNDERPROMOTES, PHASE_CASTLES, PHASE_EVASIONS, PHASE_END
} phase_t;

/*
typedef enum {
    ROOT_GENERATION, PV_GENERATION, NORMAL_GENERATION,
    ESCAPE_GENERATION, QUIESCENT_GENERATION, QUIESCENT_D0_GENERATION
} generation_t;
*/

phase_t phase_list[5][8] = {
    {PHASE_TRANS, PHASE_ROOT, PHASE_END},
    {PHASE_TRANS, PHASE_GOOD_CAPS, PHASE_KILLERS,
        PHASE_QUIET, PHASE_BAD_CAPS, PHASE_END}, 
    {PHASE_TRANS, PHASE_GOOD_CAPS, PHASE_KILLERS,
        PHASE_QUIET, PHASE_BAD_CAPS, PHASE_END}, 
    {PHASE_EVASIONS, PHASE_END},
    {PHASE_GOOD_CAPS, PHASE_END},
    {PHASE_GOOD_CAPS, PHASE_CHECKS, PHASE_END}
    //TODO: compare to 
    //{PHASE_CHECKS, PHASE_GOOD_CAPS, PHASE_END}
};

/*
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
*/

void init_move_selector(position_t* pos,
        move_selector_t* sel,
        generation_t gen_type,
        search_node_t* search_node,
        move_t hash_move,
        int depth,
        int ply)
{
    sel->pos = pos;
    if (is_check(pos)) sel->generator = ESCAPE_GENERATION;
    else sel->generator = gen_type;
    sel->hash_move = hash_move;
    sel->depth = depth;
    sel->current_move_index = 0;
    sel->current_phase_index = 0;
    sel->phase = phase_list[sel->generator][0];
    if (ply >=2) {
        killers[3] = (search_node-2)->killers[0];
        killers[4] = (search_node-2)->killers[1];
    } else {
        killers[3] = killers[4] = NO_MOVE;
    }
}

static void generate_moves(move_selector_t* sel)
{
}

move_t select_move(move_selector_t* sel)
{
    move_t move;
    while ((move = sel->moves[sel->current_move_index]) != NO_MOVE) {
    }
    // Out of moves, generate more
    sel->current_phase++;
    generate_moves(sel);
    return select_move(sel);
}

/*
 * Order moves at the root based on total nodes searched under that move.
 * This is kind of an ugly implementation, due to the way that |pick_move|
 * works, combined with the fact that node counts might overflow an int.
 */
void order_root_moves(search_data_t* root_data, move_t hash_move)
{
    move_t* moves = root_data->root_move_list.moves;
    root_data->root_move_list.offset = 0;
    uint64_t* scores = root_data->move_nodes;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        uint64_t score = scores[i];
        move_t move = moves[i];
        if (move == hash_move) score = UINT64_MAX;
        int j = i-1;
        while (j >= 0 && scores[j] < score) {
            scores[j+1] = scores[j];
            moves[j+1] = moves[j];
            --j;
        }
        scores[j+1] = score;
        moves[j+1] = move;
    }
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        root_data->root_move_list.scores[i] = INT_MAX-i;
    }
}

/*
 * Take an unordered list of pseudo-legal moves and score them according
 * to how good we think they'll be. This just identifies a few key classes
 * of moves and applies scores appropriately. Moves are then selected
 * by |pick_move|.
 * TODO: try ordering captures by mvv/lva, but categorizing
 * by see when they're searched
 */
void order_moves(position_t* pos,
        search_node_t* search_node,
        move_list_t* move_list,
        move_t hash_move,
        int ply)
{
    move_t* moves = move_list->moves;
    move_list->offset = 0;
    const int grain = MAX_HISTORY;
    const int hash_score = 1000 * grain;
    const int promote_score = 900 * grain;
    const int underpromote_score = -600 * grain;
    const int capture_score = 800 * grain;
    const int killer_score = 700 * grain;
    const int castle_score = 2*grain;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        int score = 0;
        piece_type_t promote_type = get_move_promote(move);
        if (move == hash_move) score = hash_score;
        else if (promote_type == QUEEN && static_exchange_eval(pos, move)>=0) {
            score = promote_score;
        } else if (promote_type != NONE && promote_type != QUEEN) {
            score = underpromote_score + promote_type;
        } else if (get_move_capture(move)) {
            int see = static_exchange_eval(pos, move) * grain;
            if (see >= 0) {
                score = capture_score + see;
                square_t to = get_move_to(move);
                if (ply > 0) {
                    square_t last_to = get_move_to((search_node-1)->pv[ply-1]);
                    if(to == last_to) score += grain;
                }
            } else {
                score = see;
            }
        } else if (move == search_node->killers[0]) {
            score = killer_score;
        } else if (move == search_node->killers[1]) {
            score = killer_score-1;
        } else if (ply >=2 && move == (search_node-2)->killers[0]) {
            score = killer_score-2;
        } else if (ply >=2 && move == (search_node-2)->killers[1]) {
            score = killer_score-3;
        } else if (is_move_castle(move)) {
            score = castle_score;
        } else {
            score = root_data.history.history[history_index(move)];
        }
        move_list->scores[i] = score;
    }
}

move_t pick_move(move_list_t* move_list, bool pick_best)
{
    if (!pick_best) return move_list->moves[move_list->offset++];
    move_t move = NO_MOVE;
    int score = INT_MIN;
    int index = -1;
    int offset = move_list->offset;

    for (int i=offset; move_list->moves[i] != NO_MOVE; ++i) {
        if (move_list->scores[i] > score) {
            score = move_list->scores[i];
            index = i;
        }
    }
    if (index != -1) {
        move = move_list->moves[index];
        score = move_list->scores[index];
        move_list->moves[index] = move_list->moves[offset];
        move_list->scores[index] = move_list->scores[offset];
        move_list->moves[offset] = move;
        move_list->scores[offset] = score;
        move_list->offset++;
    }
    return move;
}

//void order_qmoves(position_t* pos,
//        search_node_t* search_node,
//        move_t* moves,
//        int* scores,
//        move_t hash_move,
//        int ply)
//{
//    if (is_check(pos)) order_moves(pos, search_node, moves,
//            scores, hash_move, ply);
//    const int grain = MAX_HISTORY;
//    const int hash_score = 1000 * grain;
//    const int promote_score = 900 * grain;
//    const int capture_score = 700 * grain;
//    for (int i=0; moves[i] != NO_MOVE; ++i) {
//        const move_t move = moves[i];
//        int score = 0;
//        piece_type_t promote_type = get_move_promote(move);
//        if (move == hash_move) score = hash_score;
//        else if (promote_type == QUEEN && static_exchange_eval(pos, move)>=0) {
//            score = promote_score;
//        } else if (get_move_capture(move)) {
//            score = capture_score - get_move_piece_type(move);
//            square_t to = get_move_to(move);
//            if (ply > 0) {
//                square_t last_to = get_move_to((search_node-1)->pv[ply-1]);
//                if (to == last_to) score += grain;
//            }
//        } else {
//            score = 0;
//        }
//        // Insert the score into the right place in the list. The list is never
//        // long enough to requre an n log n algorithm.
//        int j = i-1;
//        while (j >= 0 && scores[j] < score) {
//            scores[j+1] = scores[j];
//            moves[j+1] = moves[j];
//            --j;
//        }
//        scores[j+1] = score;
//        moves[j+1] = move;
//    }
//}

