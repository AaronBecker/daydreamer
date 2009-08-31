
#include "daydreamer.h"

extern search_data_t root_data;

// TODO: implement lazy ordering
static const int ordered_pv_moves = 256;
static const int ordered_nonpv_moves = 16;
static const int ordered_quiescent_moves = 4;


//typedef int (*gen_fn)(const position_t*, move_t*);

/*
int generate_pseudo_moves(const position_t* position, move_t* move_list);
int generate_pseudo_captures(const position_t* position, move_t* move_list);
int generate_pseudo_noncaptures(const position_t* position, move_t* move_list);
int generate_quiescence_moves(const position_t* pos,
        move_t* moves,
        bool generate_checks);
int generate_evasions(const position_t* pos, move_t* moves);
*/
/*
typedef enum {
    ROOT_GENERATION, PV_GENERATION, NORMAL_GENERATION,
    ESCAPE_GENERATION, QUIESCENT_GENERATION, QUIESCENT_D0_GENERATION
} generation_t;
*/

phase_t phase_list[6][8] = {
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
    int move_end;
    int current_move_index;
    int phase_index;
    phase_t phase;
    generation_t generator;
    move_t hash_move;
    move_t killers[4];
    int depth;
    position_t* pos;
} move_selector_t;
*/

static bool generate_moves(move_selector_t* sel);
static void sort_moves(move_selector_t* sel);
static void sort_captures(move_selector_t* sel);

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
    sel->current_move_index = sel->phase_index = 0;
    sel->moves_end = sel->bad_end = 0;
    sel->phase = phase_list[sel->generator][0];
    sel->killers[0] = search_node->killers[0];
    sel->killers[1] = search_node->killers[1];
    if (ply >=2) {
        sel->killers[3] = (search_node-2)->killers[0];
        sel->killers[4] = (search_node-2)->killers[1];
    } else {
        sel->killers[3] = sel->killers[4] = NO_MOVE;
    }
    generate_moves(sel);
}

static bool generate_moves(move_selector_t* sel)
{
    sel->moves_end = 0;
    int i=0;
    switch (sel->phase) {
        case PHASE_TRANS:
            sel->moves[sel->moves_end++] = sel->hash_move;
            sel->moves[sel->moves_end] = NO_MOVE;
            break;
        case PHASE_KILLERS:
            for (; sel->killers != NO_MOVE; ++i) {
                sel->moves[sel->moves_end++] = sel->killers[i];
            }
            sel->moves[sel->moves_end] = NO_MOVE;
            break;
        case PHASE_GOOD_CAPS:
            sel->moves_end = generate_pseudo_captures(sel->pos, sel->moves);
            sort_captures(sel);
            break;
        case PHASE_BAD_CAPS:
            for (; sel->bad_captures[i] != NO_MOVE; ++i) {
                sel->moves[i] = sel->bad_captures[i];
            }
            sel->moves[i] = NO_MOVE;
            sel->moves_end = i;
            break;
        case PHASE_QUIET:
            sel->moves_end = generate_pseudo_noncaptures(sel->pos, sel->moves);
            sort_moves(sel);
            break;
        case PHASE_CHECKS:
            sel->moves_end = generate_pseudo_checks(sel->pos, sel->moves);
            sort_moves(sel);
            break;
        case PHASE_EVASIONS:
            sel->moves_end = generate_evasions(sel->pos, sel->moves);
            sort_moves(sel);
            break;
        case PHASE_ROOT:
            break;
        case PHASE_END:
            return false;
    }
    assert(sel->moves[sel->moves_end] == NO_MOVE);
    return true;
}

move_t select_move(move_selector_t* sel)
{
    move_t move;
    while ((move = sel->moves[++sel->current_move_index]) != NO_MOVE) {
        if (sel->phase!=PHASE_TRANS && move==sel->hash_move) continue;
        if (sel->phase == PHASE_QUIET &&
                move == sel->killers[0] || move == sel->killers[1] ||
                move == sel->killers[2] || move == sel->killers[3]) continue;
        if (sel->phase == PHASE_GOOD_CAPS) {
            int index = sel->current_move_index;
            if (sel->scores[index] < 0) {
                // We've gotten to the bad captures.
                int i;
                for (i=index; sel->moves[i] != NO_MOVE; ++i) {
                    sel->bad_captures[i-index] = sel->moves[i];
                }
                sel->bad_end = i-index;
                sel->bad_captures[i-index] = NO_MOVE;
                break;
            }
        }
        check_pseudo_move_legality(pos, move);
        if (sel->phase != PHASE_EVASIONS &&
                !is_pseudo_move_legal(sel->pos, move)) continue;
        assert(sel->phase != PHASE_GOOD_CAPS ||
                sel->scores[sel->current_move_index] >= 0);
        return move;
    }
    // Out of moves, generate more
    sel->phase = phase_list[sel->generator][++sel->phase_index];
    if (!generate_moves(sel)) return NO_MOVE;
    return select_move(sel);
}

// TODO: separate sorting functions for different phases.
// TODO: order captures by mva/lva within their phase
static void sort_moves(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;
    const int grain = MAX_HISTORY;
    const int hash_score = 1000 * grain;
    const int promote_score = 900 * grain;
    const int underpromote_score = -600 * grain;
    const int capture_score = 800 * grain;
    const int bad_capture_score = -100 * grain;
    const int killer_score = 700 * grain;
    const int castle_score = 2*grain;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        int score = 0;
        piece_type_t promote_type = get_move_promote(move);
        if (move == sel->hash_move) score = hash_score;
        else if (promote_type == QUEEN &&
                static_exchange_eval(sel->pos, move)>=0) {
            score = promote_score;
        } else if (promote_type != NONE && promote_type != QUEEN) {
            score = underpromote_score + promote_type;
        } else if (get_move_capture(move)) {
            int see = static_exchange_eval(sel->pos, move) * grain;
            score = see >= 0 ? capture_score + see : bad_capture_score + see;
        } else if (move == sel->killers[0]) {
            score = killer_score;
        } else if (move == sel->killers[1]) {
            score = killer_score-1;
        } else if (move == sel->killers[2]) {
            score = killer_score-2;
        } else if (move == sel->killers[3]) {
            score = killer_score-3;
        } else if (is_move_castle(move)) {
            score = castle_score;
        } else {
            score = root_data.history.history[history_index(move)];
        }
        scores[i] = score;

        int j = i-1;
        while (j >= 0 && scores[j] < score) {
            scores[j+1] = scores[j];
            moves[j+1] = moves[j];
            --j;
        }
        scores[j+1] = score;
        moves[j+1] = move;
    }
}

// TODO: order captures by mva/lva within their phase
static void sort_captures(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;
    const int grain = MAX_HISTORY;
    const int hash_score = 1000 * grain;
    const int promote_score = 900 * grain;
    const int underpromote_score = -600 * grain;
    const int capture_score = 800 * grain;
    const int bad_capture_score = -100 * grain;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        int score = 0;
        piece_type_t promote_type = get_move_promote(move);
        if (promote_type == QUEEN) {
            score = promote_score;
        } else if (promote_type != NONE && promote_type != QUEEN) {
            score = underpromote_score + promote_type;
        } else {
            piece_type_t type = get_move_piece_type(move);
            piece_type_t opp_type = piece_type(get_move_capture(move));
            if (type == PAWN || type <= opp_type) {
                score = capture_score + opp_type - type;
            } else {
                int see = static_exchange_eval(sel->pos, move) * grain;
                score = see >= 0 ?
                    capture_score + see :
                    bad_capture_score + see;
            }
        }
        scores[i] = score;

        int j = i-1;
        while (j >= 0 && scores[j] < score) {
            scores[j+1] = scores[j];
            moves[j+1] = moves[j];
            --j;
        }
        scores[j+1] = score;
        moves[j+1] = move;
    }
}


//////////////////
// Old move selection interface
//////////////////

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

