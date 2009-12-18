
#include "daydreamer.h"

extern search_data_t root_data;

selection_phase_t phase_table[6][8] = {
    { PHASE_BEGIN, PHASE_ROOT, PHASE_END },
    { PHASE_BEGIN, PHASE_PV, PHASE_END },
    { PHASE_BEGIN, PHASE_NON_PV, PHASE_END },
    { PHASE_BEGIN, PHASE_EVASIONS, PHASE_END },
    { PHASE_BEGIN, PHASE_QSEARCH, PHASE_END },
    { PHASE_BEGIN, PHASE_QSEARCH_CH, PHASE_END },
};

// How many moves should be selected by scanning through the score list and
// picking the highest available, as opposed to picking them in order? Note
// that root selection is 0 because the moves are already sorted into the
// correct order.
static const int ordered_move_count[6] = { 0, 256, 16, 16, 4, 4 };

static void generate_moves(move_selector_t* sel);
static void score_moves(move_selector_t* sel);
static void sort_root_moves(move_selector_t* sel);
static int score_tactical_move(position_t* pos, move_t move);

/*
 * Initialize the move selector data structure with the information needed to
 * determine what kind of moves to generate and how to order them.
 */
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
    sel->phase = phase_table[sel->generator];
    sel->hash_move[0] = hash_move;
    sel->hash_move[1] = NO_MOVE;
    sel->depth = depth;
    sel->moves_so_far = 0;
    sel->ordered_moves = ordered_move_count[gen_type];
    if (search_node) {
        sel->mate_killer = search_node->mate_killer;
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

/*
 * Is there only one possible move in the current position? This may need to
 * be changed for phased move generation later.
 */
bool has_single_reply(move_selector_t* sel)
{
    return sel->single_reply;
}

/*
 * Fill the list of candidate moves and score each move for later selection.
 */
static void generate_moves(move_selector_t* sel)
{
    sel->phase++;
    sel->moves_end = 0;
    sel->current_move_index = 0;
    sel->moves = sel->base_moves;
    sel->scores = sel->base_scores;
    switch (*sel->phase) {
        case PHASE_BEGIN:
            assert(false);
        case PHASE_END:
            return;
        case PHASE_TRANS:
            sel->moves = sel->hash_move;
            break;
        case PHASE_EVASIONS:
            sel->moves_end = generate_evasions(sel->pos, sel->moves);
            score_moves(sel);
            break;
        case PHASE_ROOT:
            sort_root_moves(sel);
            break;
        case PHASE_PV:
        case PHASE_NON_PV:
            sel->moves_end = generate_pseudo_moves(sel->pos, sel->moves);
            score_moves(sel);
            break;
        case PHASE_QSEARCH_CH:
            sel->moves_end = generate_quiescence_moves(
                    sel->pos, sel->moves, true);
            score_moves(sel);
            break;
        case PHASE_QSEARCH:
            sel->moves_end = generate_quiescence_moves(
                    sel->pos, sel->moves, false);
            score_moves(sel);
            break;
    }
    sel->single_reply = sel->generator == ESCAPE_GEN && sel->moves_end == 1;
    assert(sel->moves[sel->moves_end] == NO_MOVE);
    assert(sel->current_move_index == 0);
}

/*
 * Return the next move to be searched. The first n moves are returned in order
 * of their score, and the rest in the order they were generated. n depends on
 * the type of node we're at.
 */
move_t select_move(move_selector_t* sel)
{
    if (*sel->phase == PHASE_END) return NO_MOVE;

    move_t move;
    switch (*sel->phase) {
        case PHASE_TRANS:
            move = sel->hash_move[sel->current_move_index++];
            if (!move || !is_move_legal(sel->pos, move)) break;
            sel->moves_so_far++;
            return move;
        case PHASE_ROOT:
        case PHASE_PV:
        case PHASE_NON_PV:
        case PHASE_QSEARCH:
        case PHASE_QSEARCH_CH:
        case PHASE_EVASIONS:
            while (sel->current_move_index >= sel->ordered_moves) {
                move = sel->moves[sel->current_move_index++];
                if (move == NO_MOVE /*|| move == sel->hash_move[0]*/) break;
                if (sel->generator != ESCAPE_GEN &&
                        sel->generator != ROOT_GEN &&
                        !is_pseudo_move_legal(sel->pos, move)) {
                    continue;
                }
                sel->moves_so_far++;
                return move;
            }
            if (sel->current_move_index >= sel->ordered_moves) break;

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
                if (move == NO_MOVE /*|| move == sel->hash_move[0]*/) break;
                if ((sel->generator == Q_CHECK_GEN ||
                     sel->generator == Q_GEN) &&
                    (!get_move_promote(move) ||
                     get_move_promote(move) != QUEEN) &&
                    sel->scores[sel->current_move_index-1] <
                    MAX_HISTORY) continue;
                if (sel->generator != ESCAPE_GEN &&
                        sel->generator != ROOT_GEN &&
                        !is_pseudo_move_legal(sel->pos, move)) continue;
                check_pseudo_move_legality(sel->pos, move);
                sel->moves_so_far++;
                return move;
            }
            break;
        default: assert(false);
    }

    assert(*sel->phase != PHASE_END);
    generate_moves(sel);
    return select_move(sel);
}

/*
 * Take an unordered list of pseudo-legal moves and score them according
 * to how good we think they'll be. This just identifies a few key classes
 * of moves and applies scores appropriately. Moves are then selected
 * by |select_move|.
 * TODO: separate scoring functions for different phases.
 */
static void score_moves(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;

    const int grain = MAX_HISTORY;
    const int hash_score = 1000 * grain;
    const int killer_score = 700 * grain;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        int score = 0;
        if (move == sel->hash_move[0]) {
            score = hash_score;
        } else if (move == sel->mate_killer) {
            score = hash_score-1;
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
    }
}

/*
 * Determine a score for a capturing or promoting move.
 */
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
 * Sort moves at the root based on total nodes searched under that move.
 * Since the moves are sorted into position, |sel->scores| is not used to
 * select moves during root move selection.
 */
static void sort_root_moves(move_selector_t* sel)
{
    int i;
    uint64_t scores[256];
    for (i=0; root_data.root_moves[i].move != NO_MOVE; ++i) {
        sel->moves[i] = root_data.root_moves[i].move;
        if (sel->depth <= 2) {
            scores[i] = root_data.root_moves[i].qsearch_score;
        } else if (options.multi_pv > 1) {
            scores[i] = root_data.root_moves[i].score;
        } else {
            scores[i] = root_data.root_moves[i].nodes;
        }
        if (sel->moves[i] == sel->hash_move[0]) scores[i] = UINT64_MAX;
    }
    sel->moves_end = i;
    sel->moves[i] = NO_MOVE;

    for (i=0; sel->moves[i] != NO_MOVE; ++i) {
        move_t move = sel->moves[i];
        uint64_t score = scores[i];
        int j = i-1;
        while (j >= 0 && scores[j] < score) {
            scores[j+1] = scores[j];
            sel->moves[j+1] = sel->moves[j];
            --j;
        }
        scores[j+1] = score;
        sel->moves[j+1] = move;
    }
}

