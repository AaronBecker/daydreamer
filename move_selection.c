
#include "daydreamer.h"

extern search_data_t root_data;
static const int ordered_move_count[6] = { 256, 256, 16, 16, 4, 4 };

static void generate_moves(move_selector_t* sel);
static void sort_moves(move_selector_t* sel);
static void sort_tactics(move_selector_t* sel);
static void sort_root_moves(move_selector_t* sel);
static int score_tactical_move(position_t* pos, move_t move);
static void old_sort_moves(move_selector_t* sel);
static void sel_score_moves(move_selector_t* sel);

void init_move_selector(move_selector_t* sel,
        position_t* pos,
        generation_t gen_type,
        search_node_t* search_node,
        move_t hash_move,
        int depth,
        int ply)
{
    sel->pos = pos;
    if (is_check(pos) && gen_type != ROOT_GEN) {
        sel->generator = ESCAPE_GEN;
    } else {
        sel->generator = gen_type;
    }
    sel->hash_move = hash_move;
    sel->depth = depth;
    sel->moves_so_far = 0;
    sel->ordered_moves = ordered_move_count[gen_type];
    if (search_node) {
        sel->killers[0] = search_node->killers[0];
        sel->killers[1] = search_node->killers[1];
        if (ply >= 2) {
            sel->killers[2] = (search_node-2)->killers[0];
            sel->killers[3] = (search_node-2)->killers[1];
        } else {
            sel->killers[2] = sel->killers[3] = NO_MOVE;
        }
        sel->killers[4] = NO_MOVE;
    } else {
        sel->killers[0] = NO_MOVE;
    }
    generate_moves(sel);
}

bool has_single_reply(move_selector_t* sel)
{
    return sel->single_reply;
}

static void generate_moves(move_selector_t* sel)
{
    sel->moves_end = 0;
    sel->current_move_index = 0;
    switch (sel->generator) {
        case ESCAPE_GEN:
            sel->moves_end = generate_evasions(sel->pos, sel->moves);
            sel_score_moves(sel);
            //old_sort_moves(sel);
            break;
        case ROOT_GEN:
            sort_root_moves(sel);
            break;
        case PV_GEN:
        case NONPV_GEN:
            sel->moves_end = generate_pseudo_moves(sel->pos, sel->moves);
            sel_score_moves(sel);
            //old_sort_moves(sel);
            break;
        case Q_CHECK_GEN:
            sel->moves_end = generate_quiescence_moves(
                    sel->pos, sel->moves, true);
            sel_score_moves(sel);
            //old_sort_moves(sel);
            break;
        case Q_GEN:
            sel->moves_end = generate_quiescence_moves(
                    sel->pos, sel->moves, false);
            sel_score_moves(sel);
            //old_sort_moves(sel);
            break;
    }
    sel->single_reply = sel->moves_end == 1;
    assert(sel->moves[sel->moves_end] == NO_MOVE);
    assert(sel->current_move_index == 0);
}

move_t select_move(move_selector_t* sel)
{
    move_t move;
    while (sel->current_move_index >= sel->ordered_moves) {
        move = sel->moves[sel->current_move_index++];
        if (move == NO_MOVE) return move;
        if (sel->generator != ESCAPE_GEN && sel->generator != ROOT_GEN &&
                !is_pseudo_move_legal(sel->pos, move)) {
            continue;
        }
        sel->moves_so_far++;
        return move;
    }

    while (true) {
        assert(sel->current_move_index <= sel->moves_end);
        int offset = sel->current_move_index;
        move = NO_MOVE;
        int score = INT_MIN;
        int index = -1;
        for (int i=offset; sel->moves[i] != NO_MOVE; ++i) {
            if (sel->scores[i] > score) {
                score = sel->scores[i];
                index = i;
            }
        }
        if (index != -1) {
            move = sel->moves[index];
            score = sel->scores[index];
            sel->moves[index] = sel->moves[offset];
            sel->scores[index] = sel->scores[offset];
            sel->moves[offset] = move;
            sel->scores[offset] = score;
            sel->current_move_index++;
        }
        if (move == NO_MOVE) return move;
        if ((sel->generator == Q_CHECK_GEN || sel->generator == Q_GEN) &&
            (!get_move_promote(move) || get_move_promote(move) != QUEEN) &&
            sel->scores[sel->current_move_index-1] < MAX_HISTORY) continue;
                //sel->scores[sel->current_move_index-1] < 0) continue;
        if (sel->generator != ESCAPE_GEN && sel->generator != ROOT_GEN &&
                !is_pseudo_move_legal(sel->pos, move)) continue;
        check_pseudo_move_legality(sel->pos, move);
        sel->moves_so_far++;
        break;
    }
    return move;

    /*
     * NOTE: change current_move_index back to -1 in generate
    move_t move;
    while ((move = sel->moves[++(sel->current_move_index)]) != NO_MOVE) {
        assert (sel->current_move_index < sel->moves_end);
        // Only search non-losing checks in quiescence
        if ((sel->generator == Q_CHECK_GEN || sel->generator == Q_GEN) &&
                sel->scores[sel->current_move_index] < 0) continue;
        if (sel->generator != ESCAPE_GEN && sel->generator != ROOT_GEN &&
                !is_pseudo_move_legal(sel->pos, move)) continue;
        check_pseudo_move_legality(sel->pos, move);
        sel->moves_so_far++;
        return move;
    }
    return NO_MOVE;
    */
}

// TODO: separate sorting functions for different phases.
static void sort_moves(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;

    const int grain = MAX_HISTORY;
    const int hash_score = 1000 * grain;
    const int killer_score = 700 * grain;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        int score = 0;
        if (move == sel->hash_move) {
            score = hash_score;
        } else if (get_move_capture(move) || get_move_promote(move)) {
            score = score_tactical_move(sel->pos, move);
        } else if (move == sel->killers[0]) {
            score = killer_score;
        } else if (move == sel->killers[1]) {
            score = killer_score-1;
        } else if (move == sel->killers[2]) {
            score = killer_score-2;
        } else if (move == sel->killers[3]) {
            score = killer_score-3;
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

static void sort_tactics(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        const int score = score_tactical_move(sel->pos, moves[i]);
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
 * Sort moves at the root based on total nodes searched under that move.
 */
static void sort_root_moves(move_selector_t* sel)
{
    int i;
    for (i=0; root_data.moves[i] != NO_MOVE; ++i) {
        sel->moves[i] = root_data.moves[i];
    }
    sel->moves_end = i;
    sel->moves[i] = NO_MOVE;
    if (sel->depth < 3) {
        //sort_moves(sel);
        sel_score_moves(sel);
        return;
    }

    uint64_t scores[256];
    move_t* moves = sel->moves;
    for (i=0; moves[i] != NO_MOVE; ++i) scores[i] = root_data.move_nodes[i];
    for (i=0; moves[i] != NO_MOVE; ++i) {
        uint64_t score = scores[i];
        move_t move = moves[i];
        if (move == sel->hash_move) score = UINT64_MAX;
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

void store_root_node_count(move_t move, uint64_t nodes)
{
    int i;
    for (i=0; root_data.moves[i] != move &&
            root_data.moves[i] != NO_MOVE; ++i) {}
    assert(root_data.moves[i] == move);
    root_data.move_nodes[i] = nodes;
}

uint64_t get_root_node_count(move_t move)
{
    int i;
    for (i=0; root_data.moves[i] != move &&
            root_data.moves[i] != NO_MOVE; ++i) {}
    assert(root_data.moves[i] == move);
    return root_data.move_nodes[i];
}

static int score_tactical_move(position_t* pos, move_t move)
{
    const int grain = MAX_HISTORY;
    const int good_tactic_score = 800 * grain;
    const int bad_tactic_score = -800 * grain;
    bool good_tactic;
    piece_type_t piece = get_move_piece_type(move);
    piece_type_t promote = get_move_promote(move);
    piece_type_t capture = piece_type(get_move_capture(move));
    if (promote != NONE && promote != QUEEN) good_tactic = false;
    else if (capture != NONE && piece <= capture) good_tactic = true;
    else good_tactic = (static_exchange_eval(pos, move) >= 0);
    return 6*capture - piece + 5 +
        (good_tactic ? good_tactic_score : bad_tactic_score);
}

/*
 * Take an unordered list of pseudo-legal moves and score them according
 * to how good we think they'll be. This just identifies a few key classes
 * of moves and applies scores appropriately. Moves are then selected
 * by |pick_move|.
 * TODO: try ordering captures by mvv/lva, then categorizing
 * by see when they're searched
 */
void order_moves(position_t* pos,
        search_node_t* search_node,
        move_list_t* move_list,
        move_t hash_move,
        int ply)
{
    move_list->offset = 0;
    move_t* moves = move_list->moves;
    int* scores = move_list->scores;

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
        scores[i] = score;
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

static void old_sort_moves(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;

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
        if (move == sel->hash_move) score = hash_score;
        else if (promote_type == QUEEN && static_exchange_eval(sel->pos, move)>=0) {
            score = promote_score;
        } else if (promote_type != NONE && promote_type != QUEEN) {
            score = underpromote_score + promote_type;
        } else if (get_move_capture(move)) {
            int see = static_exchange_eval(sel->pos, move) * grain;
            if (see >= 0) {
                score = capture_score + see;
            } else {
                score = see;
            }
        } else if (move == sel->killers[0]) {
            score = killer_score;
        } else if (move == sel->killers[1]) {
            score = killer_score-1;
        } else if (sel->killers[2]) {
            score = killer_score-2;
        } else if (sel->killers[3]) {
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

static void sel_score_moves(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;

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
        if (move == sel->hash_move) score = hash_score;
        else if (promote_type == QUEEN &&
                static_exchange_eval(sel->pos, move)>=0) {
            score = promote_score;
        } else if (promote_type != NONE && promote_type != QUEEN) {
            score = underpromote_score + promote_type;
        } else if (get_move_capture(move)) {
            int see = static_exchange_eval(sel->pos, move) * grain;
            if (see >= 0) {
                score = capture_score + see;
            } else {
                score = see;
            }
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
    }
}
