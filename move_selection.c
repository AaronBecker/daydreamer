
#include "daydreamer.h"

extern search_data_t root_data;

#define is_trans(x)   ((x) == sel->hash_move[0])
#define is_killer(x) \
    ((x) == sel->killers[0] || \
    (x) == sel->killers[1] || \
    (x) == sel->killers[2] || \
    (x) == sel->killers[3])

const selection_phase_t phase_table[6][8] = {
    { PHASE_BEGIN, ROOT_MOVES, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, GOOD_TACTICS, KILLER_MOVES,
        QUIET_MOVES, BAD_TACTICS, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, GOOD_TACTICS, KILLER_MOVES,
        QUIET_MOVES, BAD_TACTICS, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, KILLER_MOVES, EVASIONS, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, QSEARCH_TACTICS, PHASE_END },
    { PHASE_BEGIN, TRANS_MOVE, QSEARCH_TACTICS, QSEARCH_CHECKS, PHASE_END }
};

static void generate_moves(move_selector_t* sel);
static void sort_root_moves(move_selector_t* sel);
static void sort_tactics(move_selector_t* sel);
static void sort_quiet_moves(move_selector_t* sel);
static void sort_evasions(move_selector_t* sel);
static void sort_qsearch_checks(move_selector_t* sel);
static void sort_scored_moves(move_selector_t* sel);
static void score_moves(move_selector_t* sel);
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
    sel->phase = (selection_phase_t*)phase_table[sel->generator];
    sel->hash_move[0] = hash_move;
    sel->hash_move[1] = NO_MOVE;
    sel->depth = depth;
    sel->moves_so_far = 0;
    move_t* killer = sel->killers;
    killer[0] = killer[1] = killer[2] = killer[3] = killer[4] = NO_MOVE;
    if (search_node) {
        if (search_node->killers[0]) *killer++ = search_node->killers[0];
        if (search_node->killers[1]) *killer++ = search_node->killers[1];
        if (ply >= 2) {
            if ((search_node-2)->killers[0]) {
                *killer++ = (search_node-2)->killers[0];
            }
            if ((search_node-2)->killers[1]) {
                *killer++ = (search_node-2)->killers[1];
            }
        }
    }
    assert(*sel->phase != PHASE_END);
    generate_moves(sel);
}

/*
 * Is there only one possible move in the current position? This may need to
 * be changed for phased move generation later.
 */
bool has_single_reply(move_selector_t* sel)
{
    (void)sel;
    //warn("WARNING: has_single_reply doesn't work any more");
    return false;
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
        case ROOT_MOVES:
            sort_root_moves(sel);
            sel->current_move = &sel->moves[0];
            break;
        case TRANS_MOVE:
            sel->current_move = &sel->hash_move[0];
            break;
        case KILLER_MOVES:
            sel->current_move = &sel->killers[0];
            break;
        case QSEARCH_TACTICS:
        case GOOD_TACTICS:
            generate_pseudo_tactical_moves(sel->pos, sel->moves);
            sort_tactics(sel);
            sel->current_move = &sel->moves[0];
            sel->bad_tactic_tail = sel->bad_tactics;
            sel->bad_tactics[0] = NO_MOVE;
            break;
        case BAD_TACTICS:
            sel->current_move = &sel->bad_tactics[0];
            break;
        case QUIET_MOVES:
            generate_pseudo_quiet_moves(sel->pos, sel->moves);
            sort_quiet_moves(sel);
            sel->current_move = &sel->moves[0];
            break;
        case EVASIONS:
            generate_evasions(sel->pos, sel->moves);
            sort_evasions(sel);
            sel->current_move = &sel->moves[0];
            break;
        case QSEARCH_CHECKS:
            generate_pseudo_checks(sel->pos, sel->moves);
            sort_qsearch_checks(sel);
            sel->current_move = &sel->moves[0];
            break;
        default: assert(false);
    }
    if (sel->current_move == NO_MOVE && *sel->phase != PHASE_END) {
        generate_moves(sel);
    }
}

static void sort_tactics(move_selector_t* sel)
{
    // Just MVV/LVA ordering here, SEE is applied later.
    for (int i=0; sel->moves[i]; ++i) {
        move_t move = sel->moves[i];
        sel->scores[i] = score_tactical_move(sel->pos, move);
    }
    sort_scored_moves(sel);
}

static void sort_quiet_moves(move_selector_t* sel)
{
    // Plain history ordering.
    for (int i=0; sel->moves[i]; ++i) {
        move_t move = sel->moves[i];
        sel->scores[i] = root_data.history.history[history_index(move)];
    }
    sort_scored_moves(sel);
}

static void sort_evasions(move_selector_t* sel)
{
    // Use generic scoring function.
    // TODO: fix this up to not use the outdated scoring function.
    score_moves(sel);
    sort_scored_moves(sel);
}

static void sort_qsearch_checks(move_selector_t* sel)
{
    (void)sel;
    // no-op, currently qsearch checks are unsorted
}

static void sort_scored_moves(move_selector_t* sel)
{
    for (int i=0; sel->moves[i]; ++i) {
        int score = sel->scores[i];
        move_t move = sel->moves[i];
        int j = i-1;
        while (j >= 0 && sel->scores[j] < score) {
            sel->scores[j+1] = sel->scores[j];
            sel->moves[j+1] = sel->moves[j];
            --j;
        }
        sel->scores[j+1] = score;
        sel->moves[j+1] = move;
    }
}

/*
 * Return the next move to be searched. The first n moves are returned in order
 * of their score, and the rest in the order they were generated. n depends on
 * the type of node we're at.
 */
move_t select_move(move_selector_t* sel)
{
    if (*sel->phase == PHASE_END) return NO_MOVE;
    while (*sel->current_move) {
        move_t move = *(sel->current_move++);
        switch (*sel->phase) {
            case PHASE_BEGIN:
                assert(false);
            case ROOT_MOVES:
                if (!move) break;
                sel->moves_so_far++;
                return move;
            case TRANS_MOVE:
                if (!move || !is_move_legal(sel->pos, move)) break;
                sel->moves_so_far++;
                return move;
            case KILLER_MOVES:
                if (!move || is_trans(move) ||
                        !is_move_legal(sel->pos, move)) break;
                sel->moves_so_far++;
                return move;
            case BAD_TACTICS:
                // Note: the moves that need filtered out are taken care of
                // in GOOD_TACTICS.
                if (!move) break;
                sel->moves_so_far++;
                return move;
            case EVASIONS:
                if (!move || is_trans(move) || is_killer(move)) break;
                sel->moves_so_far++;
                return move;
            case QUIET_MOVES:
                if (!move || is_trans(move) || is_killer(move) ||
                        !is_pseudo_move_legal(sel->pos, move)) break;
                sel->moves_so_far++;
                return move;
            case QSEARCH_CHECKS:
                if (!move || is_trans(move) ||
                        !is_pseudo_move_legal(sel->pos, move) ||
                        static_exchange_eval(sel->pos, move) < 0) break;
                sel->moves_so_far++;
                return move;
            case QSEARCH_TACTICS:
            case GOOD_TACTICS:
                if (!move || is_trans(move) ||
                        !is_pseudo_move_legal(sel->pos, move)) break;
                // Sort out bad tactics with negative score.
                if (sel->scores[sel->current_move-sel->moves-1] >= 0) {
                    sel->moves_so_far++;
                    return move;
                }
                *(sel->bad_tactic_tail++) = move;
                *sel->bad_tactic_tail = NO_MOVE;
                break;
            default:
                assert(false);
        }
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
    // NOTE: these values are required to make the moves fit in with the
    // other scores in |score_moves|.
    const int grain = MAX_HISTORY;
    const int good_tactic_score = 800 * grain;
    const int bad_tactic_score = -800 * grain;
    bool good_tactic;
    piece_type_t piece = get_move_piece_type(move);
    piece_type_t promote = get_move_promote(move);
    piece_type_t capture = get_move_capture_type(move);
    if (promote != NONE && promote != QUEEN) good_tactic = false;
    else if (capture != NONE && piece <= capture) good_tactic = true;
    else good_tactic = (static_exchange_eval(pos, move) >= 0);
    return 6*capture - piece +
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
    for (i=0; root_data.root_moves[i].move != NO_MOVE; ++i) {
        sel->moves[i] = root_data.root_moves[i].move;
    }
    sel->moves[i] = NO_MOVE;
    /*
    if (sel->depth < 4) {
        score_moves(sel);
        sort_scored_moves(sel);
        return;
    }

    uint64_t scores[256];
    move_t* moves = sel->moves;
    for (i=0; moves[i] != NO_MOVE; ++i) scores[i] = root_data.move_nodes[i];
    for (i=0; moves[i] != NO_MOVE; ++i) {
        uint64_t score = scores[i];
        move_t move = moves[i];
        if (move == sel->hash_move[0]) score = UINT64_MAX;
        int j = i-1;
        while (j >= 0 && scores[j] < score) {
            scores[j+1] = scores[j];
            moves[j+1] = moves[j];
            --j;
        }
        scores[j+1] = score;
        moves[j+1] = move;
    }
    */
}

/*
 * The old move scoring function, kept for reference.
 */
void score_moves_classic(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;

    const int grain = MAX_HISTORY;
    const int hash_score = 100 * grain;
    const int promote_score = 90 * grain;
    const int underpromote_score = -60 * grain;
    const int capture_score = 80 * grain;
    const int killer_score = 70 * grain;
    const int castle_score = 2*grain;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        int score = 0;
        piece_type_t promote_type = get_move_promote(move);
        if (move == sel->hash_move[0]) score = hash_score;
        else if (promote_type == QUEEN &&
                static_exchange_eval(sel->pos, move)>=0) {
            score = promote_score;
        } else if (promote_type != NONE && promote_type != QUEEN) {
            score = underpromote_score + promote_type;
        } else if (get_move_capture(move)) {
            int see = static_exchange_eval(sel->pos, move);
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

