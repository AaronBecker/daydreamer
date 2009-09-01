
#include "daydreamer.h"

extern search_data_t root_data;

// TODO: separate phase for killers, with separate legality verification
static const phase_t phase_list[7][8] = {
    {PHASE_TRANS, PHASE_ROOT, PHASE_END},           // ROOT
    {PHASE_TRANS, PHASE_GOOD_TACTICS,
        PHASE_QUIET, PHASE_BAD_TACTICS, PHASE_END}, // PV
    {PHASE_TRANS, PHASE_GOOD_TACTICS,
        PHASE_QUIET, PHASE_BAD_TACTICS, PHASE_END}, // NORMAL
    {PHASE_EVASIONS, PHASE_END},                    // ESCAPE
    {PHASE_GOOD_TACTICS, PHASE_END},                // QUIESCE
    {PHASE_GOOD_TACTICS, PHASE_CHECKS, PHASE_END},  // QUIESCE_D0
    {PHASE_TRANS, PHASE_ALL, PHASE_END}, // ALL
    //{PHASE_CHECKS, PHASE_GOOD_TACTICS, PHASE_END}
};

static const int phase_ordered_nodes[6] = { 256, 256, 16, 16, 4, 8 };

#define grain MAX_HISTORY
static int hash_score = 128 * grain;
static int good_tactic_score = 64 * grain;
static int bad_tactic_score = -64 * grain;
static int killer_score = 16 * grain;

static bool generate_moves(move_selector_t* sel);
static void sort_moves(move_selector_t* sel);
static void sort_tactics(move_selector_t* sel);
static void sort_root_moves(move_selector_t* sel);
static int score_tactical_move(position_t* pos, move_t move);
static void old_sort_moves(move_selector_t* sel);

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
    sel->moves_so_far = 0;
    sel->bad_tactics[0] = NO_MOVE;
    sel->phase = phase_list[sel->generator][0];
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
    return (sel->generator == ESCAPE_GENERATION && sel->moves_end == 1);
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
        case PHASE_GOOD_TACTICS:
            sel->moves_end = generate_pseudo_tactical_moves(sel->pos,
                    sel->moves);
            if (sel->moves_so_far < phase_ordered_nodes[sel->generator]) {
                sort_tactics(sel);
            }
            break;
        case PHASE_BAD_TACTICS:
            for (; sel->bad_tactics[i] != NO_MOVE; ++i) {
                sel->moves[i] = sel->bad_tactics[i];
            }
            sel->moves[i] = NO_MOVE;
            sel->moves_end = i;
            break;
        case PHASE_QUIET:
            sel->moves_end = generate_pseudo_quiet_moves(sel->pos, sel->moves);
            if (sel->moves_so_far < phase_ordered_nodes[sel->generator]) {
                sort_moves(sel);
            }
            break;
        case PHASE_CHECKS:
                sel->moves_end = generate_pseudo_checks(sel->pos, sel->moves);
            if (sel->moves_so_far < phase_ordered_nodes[sel->generator]) {
                sort_moves(sel);
            }
            break;
        case PHASE_EVASIONS:
            sel->moves_end = generate_evasions(sel->pos, sel->moves);
            sort_moves(sel);
            break;
        case PHASE_ROOT:
            sort_root_moves(sel);
            break;
        case PHASE_ALL:
            sel->moves_end = generate_pseudo_moves(sel->pos, sel->moves);
            //old_sort_moves(sel);
            sort_moves(sel);
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
        if (sel->phase != PHASE_TRANS && sel->phase != PHASE_EVASIONS &&
                move == sel->hash_move) continue;
        if (sel->phase == PHASE_GOOD_TACTICS) {
            int index = sel->current_move_index;
            if (sel->scores[index] < 0) {
                // We've gotten to the bad tactics.
                int i;
                for (i=index; sel->moves[i] != NO_MOVE; ++i) {
                    sel->bad_tactics[i-index] = sel->moves[i];
                }
                sel->bad_end = i-index;
                sel->bad_tactics[i-index] = NO_MOVE;
                break;
            }
        }
        // Only search non-losing checks in quiescence
        if (sel->phase == PHASE_CHECKS &&
                sel->scores[sel->current_move_index] < 0) continue;
        if (sel->phase != PHASE_EVASIONS && sel->phase != PHASE_ROOT &&
                !is_pseudo_move_legal(sel->pos, move)) continue;
        assert(sel->phase != PHASE_GOOD_TACTICS ||
                sel->scores[sel->current_move_index] >= 0);
        check_pseudo_move_legality(sel->pos, move);
        sel->moves_so_far++;
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

// TODO: order tactics by mva/lva within their phase
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
        sort_moves(sel);
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


static void old_sort_moves(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;
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
                //square_t to = get_move_to(move);
                //if (ply > 0) {
                //    square_t last_to = get_move_to((search_node-1)->pv[ply-1]);
                //    if(to == last_to) score += grain;
                //}
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
