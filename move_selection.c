
#include "daydreamer.h"

extern search_data_t root_data;

#define is_trans(x)   ((x) == sel->hash_move[0])

const selection_phase_t phase_table[6][8] = {
    { PHASE_BEGIN, ROOT_MOVES, PHASE_END },

    { PHASE_BEGIN, TRANS_MOVE, GENERIC_MOVES, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, GENERIC_MOVES, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, EVASIONS, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, GENERIC_QSEARCH, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, GENERIC_QSEARCH_CH, PHASE_END },

//    { PHASE_BEGIN, TRANS_MOVE, GOOD_TACTICS,
//        QUIET_MOVES, BAD_TACTICS, PHASE_END },
//    { PHASE_BEGIN, TRANS_MOVE, GOOD_TACTICS,
//        QUIET_MOVES, BAD_TACTICS, PHASE_END },
//    { PHASE_BEGIN, TRANS_MOVE, EVASIONS, PHASE_END },
//    { PHASE_BEGIN, TRANS_MOVE, QSEARCH_TACTICS, PHASE_END },
//    { PHASE_BEGIN, TRANS_MOVE, QSEARCH_TACTICS, QSEARCH_CHECKS, PHASE_END }
};

// How many moves should be selected by scanning through the score list and
// picking the highest available, as opposed to picking them in order? Note
// that root selection is 0 because the moves are already sorted into the
// correct order.
static const int ordered_move_count[6] = { 0, 256, 16, 16, 4, 4 };

static void generate_moves(move_selector_t* sel);
static void sort_root_moves(move_selector_t* sel);
static int score_tactical_move(position_t* pos, move_t move);
static move_t choose_next_move(move_selector_t* sel, int* score);
static void score_moves_generic(move_selector_t* sel);
static void score_tactics(move_selector_t* sel);
static void score_quiet_moves(move_selector_t* sel);

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
    sel->phase = (selection_phase_t*)phase_table[sel->generator];
    sel->hash_move[0] = hash_move;
    sel->hash_move[1] = NO_MOVE;
    sel->depth = depth;
    sel->moves_so_far = 0;
    sel->ordered_moves = ordered_move_count[gen_type];
    move_t* killer = sel->killers;
    if (search_node) {
        sel->mate_killer = search_node->mate_killer;
        *killer = search_node->killers[0];
        if (*killer) {
            killer++;
            *killer = search_node->killers[0];
        }
        if (ply >= 2) {
            if (*killer) killer++;
            *killer = (search_node-2)->killers[0];
            if (*killer) {
                killer++;
                *killer = (search_node-2)->killers[1];
            }
        }
        *(killer+1) = NO_MOVE;
    } else {
        *killer = NO_MOVE;
    }
    generate_moves(sel);
}

/*
 * Is there only one possible move in the current position? This may need to
 * be changed for phased move generation later.
 */
bool has_single_reply(move_selector_t* sel)
{
    return false;
    //return is_check(sel->pos) && sel->moves[1] == NO_MOVE;
}

/*
 * Fill the list of candidate moves and score each move for later selection.
 */
static void generate_moves(move_selector_t* sel)
{
    sel->phase++;
    switch (*sel->phase) {
        case PHASE_BEGIN: assert(false);
        case PHASE_END: return;

        case GENERIC_MOVES:
            sel->moves_end_index = generate_pseudo_moves(sel->pos, sel->moves);
            sel->current_move_list = sel->moves;
            score_moves_generic(sel);
            break;
        case GENERIC_QSEARCH:
            sel->moves_end_index = generate_quiescence_moves(
                    sel->pos, sel->moves, false);
            sel->current_move_list = sel->moves;
            score_moves_generic(sel);
            break;
        case GENERIC_QSEARCH_CH:
            sel->moves_end_index = generate_quiescence_moves(
                    sel->pos, sel->moves, true);
            sel->current_move_list = sel->moves;
            score_moves_generic(sel);
            break;

        case ROOT_MOVES:
            sort_root_moves(sel);
            sel->current_move_list = sel->moves;
            break;
        case TRANS_MOVE:
            sel->current_move_list = sel->hash_move;
            break;
            /*
        case QSEARCH_TACTICS:
        case GOOD_TACTICS:
            sel->moves_end_index =
                generate_pseudo_tactical_moves(sel->pos, sel->moves);
            sel->current_move_list = sel->moves;
            sel->bad_tactic_tail = sel->bad_tactics;
            sel->bad_tactics[0] = NO_MOVE;
            score_tactics(sel);
            break;
        case BAD_TACTICS:
            sel->current_move_list = sel->bad_tactics;
            break;
        case QUIET_MOVES:
            sel->moves_end_index =
                generate_pseudo_quiet_moves(sel->pos, sel->moves);
            sel->current_move_list = sel->moves;
            score_quiet_moves(sel);
            break;
            */
        case EVASIONS:
            sel->moves_end_index = generate_evasions(sel->pos, sel->moves);
            sel->current_move_list = sel->moves;
            score_moves_generic(sel);
            break;
            /*
        case QSEARCH_CHECKS:
            sel->moves_end_index = generate_pseudo_checks(sel->pos, sel->moves);
            sel->current_move_list = sel->moves;
            score_moves_generic(sel);
            break;
            */
        default: assert(false);
    }
    if (sel->current_move_list == NULL && *sel->phase != PHASE_END) {
        generate_moves(sel);
    }
    /*
    sel->moves_end = 0;
    sel->current_move_index = 0;
    switch (sel->generator) {
        case ESCAPE_GEN:
            sel->moves_end = generate_evasions(sel->pos, sel->moves);
            score_moves(sel);
            break;
        case ROOT_GEN:
            sort_root_moves(sel);
            break;
        case PV_GEN:
        case NONPV_GEN:
            sel->moves_end = generate_pseudo_moves(sel->pos, sel->moves);
            score_moves(sel);
            break;
        case Q_CHECK_GEN:
            sel->moves_end = generate_quiescence_moves(
                    sel->pos, sel->moves, true);
            score_moves(sel);
            break;
        case Q_GEN:
            sel->moves_end = generate_quiescence_moves(
                    sel->pos, sel->moves, false);
            score_moves(sel);
            break;
    }
    sel->single_reply = sel->generator == ESCAPE_GEN && sel->moves_end == 1;
    assert(sel->moves[sel->moves_end] == NO_MOVE);
    assert(sel->current_move_index == 0);
    */
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
    int score;
    while ((move = choose_next_move(sel, &score))) {
        switch (*sel->phase) {
            case PHASE_BEGIN:
                assert(false);
            case ROOT_MOVES:
                if (!move) break;
                sel->moves_so_far++;
                return move;
            case TRANS_MOVE:
                if (!move || !is_pseudo_move_legal(sel->pos, move)) break;
                sel->moves_so_far++;
                return move;

            case GENERIC_MOVES:
                if (!move || is_trans(move) ||
                        !is_pseudo_move_legal(sel->pos, move)) break;
                sel->moves_so_far++;
                return move;
            case GENERIC_QSEARCH:
            case GENERIC_QSEARCH_CH:
                if (!move || is_trans(move) ||
                        !is_pseudo_move_legal(sel->pos, move)) break;
                if ((!get_move_promote(move) ||
                        get_move_promote(move) != QUEEN) &&
                    score < MAX_HISTORY) break;
                sel->moves_so_far++;
                return move;

                /*
            case BAD_TACTICS:
                // Note: the moves that need filtered out are taken care of
                // in GOOD_TACTICS.
                if (!move) break;
                sel->moves_so_far++;
                return move;
                */
            case EVASIONS:
                if (!move || is_trans(move)) break;
                sel->moves_so_far++;
                return move;
                /*
            case QUIET_MOVES:
                if (!move || is_trans(move) ||
                        !is_pseudo_move_legal(sel->pos, move)) break;
                sel->moves_so_far++;
                return move;
            case QSEARCH_CHECKS:
                if (!move || is_trans(move) ||
                        !is_pseudo_move_legal(sel->pos, move)) break;
                sel->moves_so_far++;
                return move;
            case QSEARCH_TACTICS:
            case GOOD_TACTICS:
                if (!move || is_trans(move) ||
                        !is_pseudo_move_legal(sel->pos, move)) break;
                // Sort out bad tactics with negative see score.
                if (static_exchange_eval(sel->pos, move) >= 0) {
                    sel->moves_so_far++;
                    return move;
                }
                *(sel->bad_tactic_tail++) = move;
                *sel->bad_tactic_tail = NO_MOVE;
                break;
                */
            default:
                assert(false);
        }
    }
    assert(*sel->phase != PHASE_END);
    generate_moves(sel);
    return select_move(sel);

    /*
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
        if (sel->generator != ESCAPE_GEN && sel->generator != ROOT_GEN &&
                !is_pseudo_move_legal(sel->pos, move)) continue;
        check_pseudo_move_legality(sel->pos, move);
        sel->moves_so_far++;
        break;
    }
    return move;
    */
}

static move_t choose_next_move(move_selector_t* sel, int* score)
{
    move_t best_move = sel->current_move_list[0];
    if (*sel->phase != ROOT_MOVES &&
            *sel->phase != TRANS_MOVE &&
            *sel->phase != BAD_TACTICS) {
        int best_score = INT_MIN;
        int best_index = 0;
        assert(sel->current_move_list == sel->moves);
        for (int i=0; sel->moves[i]; ++i) {
            if (sel->scores[i] > best_score) {
                best_move = sel->moves[i];
                best_score = sel->scores[i];
                best_index = i;
            }
        }
        sel->moves_end_index--;
        *score = sel->scores[best_index];
        sel->moves[best_index] = sel->moves[sel->moves_end_index];
        sel->scores[best_index] = sel->scores[sel->moves_end_index];
        sel->moves[sel->moves_end_index] = NO_MOVE;
    } else {
        *score = sel->scores[0];
        sel->current_move_list++;
    }
    return best_move;
}

/*
 * Take an unordered list of pseudo-legal moves and score them according
 * to how good we think they'll be. This just identifies a few key classes
 * of moves and applies scores appropriately. Moves are then selected
 * by |select_move|.
 * TODO: separate scoring functions for different phases.
 */
static void score_moves_generic(move_selector_t* sel)
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

static void score_tactics(move_selector_t* sel)
{
    for (int i=0; sel->moves[i]; ++i) {
        move_t move = sel->moves[i];
        piece_type_t piece = get_move_piece_type(move);
        piece_type_t promote = get_move_promote(move);
        piece_type_t capture = piece_type(get_move_capture(move));
        int promote_value = 0;
        if (promote == QUEEN) promote_value = 900;
        else if (promote != NONE) promote_value = -900;
        sel->scores[i] = 6*capture - piece + promote_value;
    }
}

static void score_quiet_moves(move_selector_t* sel)
{
    const int killer_score = 2 * MAX_HISTORY;
    for (int i=0; sel->moves[i]; ++i) {
        move_t move = sel->moves[i];
        int score;
        if (move == sel->mate_killer) {
            score = killer_score + 1;
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
        sel->scores[i] = score;
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
    //sel->moves_end = i;
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

