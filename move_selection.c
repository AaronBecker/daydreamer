
#include "daydreamer.h"

extern search_data_t root_data;

// TODO: implement lazy ordering
static const int ordered_pv_moves = 256;
static const int ordered_nonpv_moves = 16;
static const int ordered_quiescent_moves = 4;

//typedef enum {
//    ROOT_GENERATION, PV_GENERATION, NORMAL_GENERATION,
//    ESCAPE_GENERATION, QUIESCENT_GENERATION, QUIESCENT_D0_GENERATION
//} generation_t;

// TODO: separate phase for killers, with separate legality verification
// TODO: queen promotions in qsearch
phase_t phase_list[6][8] = {
    {PHASE_TRANS, PHASE_ROOT, PHASE_END},
//    {PHASE_TRANS, PHASE_GOOD_CAPS, PHASE_KILLERS,
//        PHASE_QUIET, PHASE_BAD_CAPS, PHASE_END}, 
    {PHASE_TRANS, PHASE_GOOD_CAPS,
        PHASE_QUIET, PHASE_BAD_CAPS, PHASE_END}, 
//    {PHASE_TRANS, PHASE_GOOD_CAPS, PHASE_KILLERS,
//        PHASE_QUIET, PHASE_BAD_CAPS, PHASE_END}, 
    {PHASE_TRANS, PHASE_GOOD_CAPS,
        PHASE_QUIET, PHASE_BAD_CAPS, PHASE_END}, 
    {PHASE_TRANS, PHASE_EVASIONS, PHASE_END},
    {PHASE_GOOD_CAPS, PHASE_END},
    {PHASE_CHECKS, PHASE_GOOD_CAPS, PHASE_END}
    //TODO: compare to 
    //{PHASE_GOOD_CAPS, PHASE_CHECKS, PHASE_END}
};

static bool generate_moves(move_selector_t* sel);
static void sort_moves(move_selector_t* sel);
static void sort_captures(move_selector_t* sel);
static bool is_killer_plausible(position_t* pos, move_t killer);

void init_move_selector(move_selector_t* sel,
        position_t* pos,
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
    sel->phase_index = 0;
    sel->bad_end = 0;
    sel->bad_captures[0] = NO_MOVE;
    sel->phase = phase_list[sel->generator][0];
    sel->killers[0] = search_node->killers[0];
    sel->killers[1] = search_node->killers[1];
    if (ply >=2) {
        sel->killers[2] = (search_node-2)->killers[0];
        sel->killers[3] = (search_node-2)->killers[1];
    } else {
        sel->killers[2] = sel->killers[3] = NO_MOVE;
    }
    sel->killers[4] = NO_MOVE;
    generate_moves(sel);
}

// FIXME: doesn't work because of PHASE_TRANS
bool has_single_reply(move_selector_t* sel)
{
    return false;
    //return (sel->generator == ESCAPE_GENERATION && sel->moves_end == 1);
}

static bool generate_moves(move_selector_t* sel)
{
    sel->moves_end = 0;
    sel->current_move_index = -1;
    int i=0;
    switch (sel->phase) {
        case PHASE_TRANS:
            sel->moves[sel->moves_end++] = sel->hash_move;
            sel->moves[sel->moves_end] = NO_MOVE;
            break;
//        case PHASE_KILLERS:
//            for (; sel->killers[i] != NO_MOVE; ++i) {
//                sel->moves[i] = sel->killers[i];
//            }
//            sel->moves[i] = NO_MOVE;
//            sel->moves_end = i;
//            break;
        case PHASE_GOOD_CAPS:
            sel->moves_end = generate_pseudo_captures(sel->pos, sel->moves);
            sort_captures(sel);
            //sort_moves(sel);
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
    assert(sel->current_move_index == -1);
    return true;
}

move_t select_move(move_selector_t* sel)
{
    move_t move;
    while ((move = sel->moves[++(sel->current_move_index)] ) != NO_MOVE) {
        assert (sel->current_move_index < sel->moves_end);
        if (sel->phase!=PHASE_TRANS && move==sel->hash_move) continue;
//        if (sel->phase == PHASE_KILLERS &&
//                !is_killer_plausible(sel->pos, move)) continue;
//        if (sel->phase == PHASE_QUIET &&
//                (move == sel->killers[0] || move == sel->killers[1] ||
//                move == sel->killers[2] || move == sel->killers[3])) continue;
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
        // Only search non-losing checks in quiescence
        if (sel->phase == PHASE_CHECKS &&
                sel->scores[sel->current_move_index] < 0) continue;
        if (sel->phase != PHASE_EVASIONS &&
                !is_pseudo_move_legal(sel->pos, move)) continue;
        assert(sel->phase != PHASE_GOOD_CAPS ||
                sel->scores[sel->current_move_index] >= 0);
        check_pseudo_move_legality(sel->pos, move);
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
            int see = static_exchange_eval(sel->pos, move) * grain;
            score = see >= 0 ? capture_score + see : bad_capture_score + see;
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

/*
 * Trying to re-use killers from previous plies will yield moves that are
 * no longer pseudo-legal because of the previous move. 
 */
static bool is_killer_plausible(position_t* pos, move_t killer)
{
    if (is_move_castle(killer)) {
        color_t side = pos->side_to_move;
        square_t king_home = E1 + side * A8;
        if (is_move_castle_short(killer) &&
            pos->board[king_home+1] == EMPTY &&
            pos->board[king_home+2] == EMPTY &&
            !is_square_attacked(pos,king_home+1,side^1)) return true;
        if (is_move_castle_long(killer) &&
            pos->board[king_home-1] == EMPTY &&
            pos->board[king_home-2] == EMPTY &&
            pos->board[king_home-3] == EMPTY &&
            !is_square_attacked(pos,king_home-1,side^1)) return true;
        return false;
    }
    square_t from = get_move_from(killer);
    square_t to = get_move_to(killer);
    return (pos->board[from] == get_move_piece(killer) &&
            pos->board[to] == EMPTY);
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

