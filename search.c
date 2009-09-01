
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "daydreamer.h"

search_data_t root_data;
static const bool nullmove_enabled = true;
static const bool verification_enabled = true;
static const bool iid_enabled = true;
static const bool razoring_enabled = false;
static const bool futility_enabled = true;
static const bool history_prune_enabled = true;
static const bool value_prune_enabled = true;
static const bool qfutility_enabled = true;
static const bool lmr_enabled = true;
static const int futility_margin[FUTILITY_DEPTH_LIMIT] = {
    100
//    100, 200
//    100, 200, 350
//    90, 190, 250, 330, 400
};
static const int qfutility_margin = 80;
static const int razor_attempt_margin[RAZOR_DEPTH_LIMIT] = {
//    400
    400, 300
//    500, 300, 300
};
static const int razor_cutoff_margin[RAZOR_DEPTH_LIMIT] = {
//    150
    150, 300
//    150, 300, 300
};

static bool should_stop_searching(search_data_t* data);
static bool root_search(search_data_t* search_data);
static int search(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth);
static int quiesce(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth);

/*
 * Zero out all search variables prior to starting a search. Leaves the
 * position and search options untouched.
 */
void init_search_data(search_data_t* data)
{
    memset(&data->stats, 0, sizeof(search_stats_t));
    //memset(&data->root_move_list, 0, sizeof(move_list_t));
    memset(&data->moves, 0, 256 * sizeof(move_t));
    memset(&data->move_nodes, 0, 256 * sizeof(uint64_t));
    data->best_move = NO_MOVE;
    data->best_score = 0;
    memset(data->pv, 0, sizeof(move_t) * MAX_SEARCH_DEPTH);
    memset(data->search_stack, 0, sizeof(search_node_t) * MAX_SEARCH_DEPTH);
    memset(&data->history, 0, sizeof(history_t));
    data->nodes_searched = 0;
    data->qnodes_searched = 0;
    data->current_depth = 0;
    data->engine_status = ENGINE_IDLE;
    init_timer(&data->timer);
    data->node_limit = 0;
    data->depth_limit = 0;
    data->time_limit = 0;
    data->time_target = 0;
    data->mate_search = 0;
    data->infinite = 0;
    data->ponder = 0;
}

/*
 * Copy pv from a deeper search node, adding a new move at the current ply.
 */
static void update_pv(move_t* dst, move_t* src, int ply, move_t move)
{
    dst[ply] = move;
    int i=ply;
    do {
        ++i;
        dst[i] = src[i];
    } while (src[i] != NO_MOVE);
}

/*
 * Every time a node is expanded, this function increments the node counter.
 * Every POLL_INTERVAL nodes, user input, is checked.
 */
static void open_node(search_data_t* data, int ply)
{
    if ((++data->nodes_searched & POLL_INTERVAL) == 0) {
        if (should_stop_searching(data)) data->engine_status = ENGINE_ABORTED;
        uci_check_input(data);
    }
    data->search_stack[ply].killers[0] = NO_MOVE;
    data->search_stack[ply].killers[1] = NO_MOVE;
}

/*
 * Open a node in quiescent search.
 */
static void open_qnode(search_data_t* data, int ply)
{
    open_node(data, ply);
    ++data->qnodes_searched;
}

/*
 * Should we terminate the search? This considers time and node limits, as
 * well as user input. This function is checked periodically during search.
 */
static bool should_stop_searching(search_data_t* data)
{
    if (data->engine_status == ENGINE_ABORTED) return true;
    if (!data->infinite &&
            data->time_target &&
            elapsed_time(&data->timer) >= data->time_target) return true;
    // TODO: take time_limit and search difficulty into account
    if (data->node_limit &&
            data->nodes_searched >= data->node_limit) return true;
    // TODO: we need a heuristic for when the current result is "good enough",
    // regardless of search params.
    return false;
}

/*
 * Should the search depth be extended? Note that our move has
 * already been played in |pos|. For now, just extend one ply on checks
 * and pawn pushes to the 7th (relative) rank.
 * Note: |move| has already been made in |pos|. We need both anyway for
 * efficiency.
 * TODO: recapture extensions might be good. Also, fractional extensions,
 * and fractional plies in general.
 */
static int extend(position_t* pos, move_t move, bool single_reply)
{
    if (is_check(pos) || single_reply) return 1;
    square_t sq = get_move_to(move);
    if (piece_type(pos->board[sq]) == PAWN &&
            (square_rank(sq) == RANK_7 || square_rank(sq) == RANK_2)) return 1;
    return 0;
}

/*
 * Should we go on to the next level of iterative deepening in our root
 * search? This considers regular stopping conditions and also guesses whether
 * or not we can finish the next depth in the remaining time.
 */
static bool should_deepen(search_data_t* data)
{
    if (should_stop_searching(data)) return false;
    // If we're more than halfway through our time, we won't make it through
    // the next iteration anyway.
    // TODO: this margin could be tightened up.
    if (!data->infinite && data->time_target &&
        data->time_target-elapsed_time(&data->timer) <
        data->time_target/2) return false;
    return true;
}

/*
 * In the given position, is the nullmove heuristic valid? We avoid nullmoves
 * in cases where we're down to king and pawns because of zugzwang.
 */
static bool is_nullmove_allowed(position_t* pos)
{
    // don't allow nullmove if either side is in check
    if (is_check(pos)) return false;
    // allow nullmove if we're not down to king/pawns
    return !(pos->num_pieces[WHITE] == 1 && pos->num_pieces[BLACK] == 1);
}

static void record_success(history_t* h, move_t move, int depth)
{
    int index = history_index(move);
    h->history[index] += depth_to_history(depth);
    h->success[index]++;

    // Keep history values inside the correct range.
    if (h->history[index] > MAX_HISTORY) {
        for (int i=0; i<MAX_HISTORY_INDEX; ++i) {
            h->history[i] /= 2;
        }
    }
}

static void record_failure(history_t* h, move_t move)
{
    h->failure[history_index(move)]++;
}

static bool is_history_prune_allowed(history_t* h, move_t move, int depth)
{
    int index = history_index(move);
    return (depth * h->success[index] < h->failure[index]);
}

static bool is_history_reduction_allowed(history_t* h, move_t move)
{
    int index = history_index(move);
    return (h->success[index] / 2 < h->failure[index]);
}

/*
 * Can we do internal iterative deepening?
 */
static bool is_iid_allowed(bool full_window, int depth)
{
    search_options_t* options = &root_data.options;
    if (full_window) {
        if (!options->enable_pv_iid ||
                options->iid_pv_depth_cutoff >= depth) return false;
    }
    if (!full_window) {
        if (!options->enable_non_pv_iid ||
                options->iid_non_pv_depth_cutoff >= depth) return false;
    }
    return true;
}

/*
 * Does the transposition table entry we found cause a cutoff?
 */
static bool is_trans_cutoff_allowed(transposition_entry_t* entry,
        int depth,
        int alpha,
        int beta)
{
    if (depth > entry->depth) return false;
    if (entry->score_type != SCORE_UPPERBOUND && entry->score > alpha) {
        alpha = entry->score;
    }
    if (entry->score_type != SCORE_LOWERBOUND && entry->score < beta) {
        beta = entry->score;
    }
    return alpha >= beta;
}


/*
 * Iterative deepening search of the root position. This is the external
 * function that is called by the console interface. For each depth,
 * |root_search| performs the actual search.
 */
void deepening_search(search_data_t* search_data)
{
    search_data->engine_status = ENGINE_THINKING;
    increment_transposition_age();
    init_timer(&search_data->timer);
    start_timer(&search_data->timer);
    // If |search_data| already has a list of root moves, we search only
    // those moves. Otherwise, search everything. This allows support for the
    // uci searchmoves command.
    if (search_data->moves[0] == NO_MOVE) {
        generate_legal_moves(&search_data->root_pos,
                search_data->moves);
    }

    int id_score = root_data.best_score = mated_in(-1);
    if (!search_data->depth_limit) search_data->depth_limit = MAX_SEARCH_DEPTH;
    for (search_data->current_depth=1;
            search_data->current_depth <= search_data->depth_limit;
            ++search_data->current_depth) {
        if (should_output(search_data)) {
            print_transposition_stats();
            printf("info depth %d\n", search_data->current_depth);
        }
        bool no_abort = root_search(search_data);
        if (!no_abort) break;
        put_transposition_line(&search_data->root_pos,
                search_data->pv,
                search_data->current_depth,
                search_data->best_score);
        id_score = search_data->best_score;
        if (!should_deepen(search_data)) {
            ++search_data->current_depth;
            break;
        }
    }
    stop_timer(&search_data->timer);

    --search_data->current_depth;
    search_data->best_score = id_score;
    print_search_stats(search_data);
    printf("info string targettime %d elapsedtime %d\n",
            search_data->time_target, elapsed_time(&search_data->timer));
    print_transposition_stats();
    print_pawn_stats();
    char coord_move[6];
    move_to_coord_str(search_data->best_move, coord_move);
    print_pv(search_data);
    printf("bestmove %s\n", coord_move);
    search_data->engine_status = ENGINE_IDLE;
}

/*
 * Perform search at the root position. |search_data| contains all relevant
 * search information, which is set in |deepening_search|.
 * TODO: aspiration window?
 */
static bool root_search(search_data_t* search_data)
{
    int alpha = mated_in(-1), beta = mate_in(-1);
    search_data->best_score = alpha;
    position_t* pos = &search_data->root_pos;
    transposition_entry_t* trans_entry = get_transposition(pos);
    move_t hash_move = trans_entry ? trans_entry->move : NO_MOVE;

    move_selector_t selector;
    init_move_selector(&selector, pos, ROOT_GENERATION,
            NULL, hash_move, search_data->current_depth, 0);
    int move_index = 0;
    for (move_t move = select_move(&selector); move != NO_MOVE;
            move = select_move(&selector), ++move_index) {
        if (should_output(search_data)) {
            char coord_move[6];
            move_to_coord_str(move, coord_move);
            printf("info currmove %s currmovenumber %d score %"PRIu64"\n",
                    coord_move, move_index, get_root_node_count(move));
        }
        uint64_t nodes_before = search_data->nodes_searched;
        undo_info_t undo;
        do_move(pos, move, &undo);
        int ext = extend(pos, move, false);
        int score;
        if (move_index == 0) {
            // First move, use full window search.
            score = -search(pos, search_data->search_stack,
                    1, -beta, -alpha, search_data->current_depth+ext-1);
        } else {
            score = -search(pos, search_data->search_stack,
                    1, -alpha-1, -alpha, search_data->current_depth+ext-1);
            if (score > alpha) {
                if (should_output(search_data)) {
                    char coord_move[6];
                    move_to_coord_str(move, coord_move);
                    printf("info string fail high, research %s\n", coord_move);
                }
                score = -search(pos, search_data->search_stack,
                        1, -beta, -alpha, search_data->current_depth+ext-1);
            }
        }
        store_root_node_count(move, search_data->nodes_searched-nodes_before);
        undo_move(pos, move, &undo);
        if (search_data->engine_status == ENGINE_ABORTED) return false;
        if (score > alpha) {
            alpha = score;
            if (score > search_data->best_score) {
                search_data->best_score = score;
                search_data->best_move = move;
            }
            update_pv(search_data->pv, search_data->search_stack->pv, 0, move);
            check_line(pos, search_data->pv);
            print_pv(search_data);
        }
    }
    return true;
}

/*
 * Search an interior, non-quiescent node.
 */
static int search(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth)
{
    search_node->pv[ply] = NO_MOVE;
    if (root_data.engine_status == ENGINE_ABORTED) return 0;
    if (alpha > mate_in(ply)) return alpha; // Can't beat this...
    if (depth <= 0) {
        return quiesce(pos, search_node, ply, alpha, beta, depth);
    }
    if (is_draw(pos)) return DRAW_VALUE;
    bool full_window = (beta-alpha > 1);

    // Put some bounds on how good/bad this node could turn out to be.
    int orig_alpha = alpha;
    alpha = MAX(alpha, mated_in(ply));
    beta = MIN(beta, mate_in(ply));
    if (alpha >= beta) return alpha;

    // Get move from transposition table if possible.
    transposition_entry_t* trans_entry = get_transposition(pos);
    move_t hash_move = trans_entry ? trans_entry->move : NO_MOVE;
    if (!full_window && trans_entry &&
            is_trans_cutoff_allowed(trans_entry, depth, alpha, beta)) {
            search_node->pv[ply] = hash_move;
            search_node->pv[ply+1] = NO_MOVE;
            root_data.stats.cutoffs[root_data.current_depth]++;
            return MAX(alpha, trans_entry->score);
    }

    open_node(&root_data, ply);
    int score = mated_in(-1);
    int lazy_score = simple_eval(pos);
    bool mate_threat = false;
    // Nullmove reduction.
    if (nullmove_enabled &&
            depth != 1 &&
            !full_window &&
            pos->prev_move != NULL_MOVE &&
            lazy_score + NULL_EVAL_MARGIN > beta &&
            !is_mate_score(beta) &&
            is_nullmove_allowed(pos)) {
        undo_info_t undo;
        do_nullmove(pos, &undo);
        int null_score = -search(pos, search_node+1, ply+1,
                -beta, -beta+1, MAX(depth-NULL_R, 0));
        undo_nullmove(pos, &undo);
        if (is_mated_score(null_score)) mate_threat = true;
        if (null_score >= beta) {
            if (verification_enabled) {
                int rdepth = depth - NULLMOVE_VERIFICATION_REDUCTION;
                if (rdepth <= 0) return beta;
                null_score = search(pos, search_node, ply, alpha, beta, rdepth);
            }
            if (null_score >= beta) return beta;
        }
    } else if (razoring_enabled &&
            pos->prev_move != NULL_MOVE &&
            !full_window &&
            depth <= RAZOR_DEPTH_LIMIT &&
            hash_move == NO_MOVE &&
            !is_mate_score(beta) &&
            lazy_score + razor_attempt_margin[depth-1] < beta) {
        // Razoring. The two-level depth-sensitive margin idea comes
        // from Stockfish.
        root_data.stats.razor_attempts[depth-1]++;
        int qscore = quiesce(pos, search_node, ply, alpha, beta, 0);
        if (qscore + razor_cutoff_margin[depth-1] < beta) {
            root_data.stats.razor_prunes[depth-1]++;
            return qscore;
        }
    }

    // Internal iterative deepening.
    if (iid_enabled &&
            hash_move == NO_MOVE &&
            is_iid_allowed(full_window, depth)) {
        const int iid_depth = depth -
            (full_window ? root_data.options.iid_pv_depth_reduction :
                           root_data.options.iid_non_pv_depth_reduction);
        assert(iid_depth > 0);
        search(pos, search_node, ply, alpha, beta, iid_depth);
        hash_move = search_node->pv[ply];
    }

    move_list_t move_list;
    move_t searched_moves[256];
    bool single_reply = generate_pseudo_moves(pos, move_list.moves) == 1;
    int num_legal_moves = 0, num_futile_moves = 0, num_searched_moves = 0;
    int ordered_moves = full_window ? 256 : 16;
    int futility_score = mated_in(-1);
    int move_index = 0;
    order_moves(pos, search_node, &move_list, hash_move, ply);
    for (move_t move = pick_move(&move_list, move_index<ordered_moves);
            move != NO_MOVE;
            move = pick_move(&move_list, move_index<ordered_moves),
            ++move_index) {
        check_pseudo_move_legality(pos, move);
        if (!is_pseudo_move_legal(pos, move)) continue;
        ++num_legal_moves;

//    move_t searched_moves[256];
//    move_selector_t selector;
//    //generation_t gen_type = ALL_GENERATION;
//    generation_t gen_type = full_window ? PV_GENERATION : NORMAL_GENERATION;
//    init_move_selector(&selector, pos, gen_type,
//            search_node, hash_move, depth, ply);
//    bool single_reply = has_single_reply(&selector);
//    int num_legal_moves = 0, num_futile_moves = 0, num_searched_moves = 0;
//    int futility_score = mated_in(-1);
//    for (move_t move = select_move(&selector); move != NO_MOVE;
//            move = select_move(&selector)) {
//        num_legal_moves = selector.moves_so_far;

        undo_info_t undo;
        do_move(pos, move, &undo);
        int ext = extend(pos, move, single_reply);
        if (num_legal_moves == 1) {
            // First move, use full window search.
            score = -search(pos, search_node+1, ply+1,
                    -beta, -alpha, depth+ext-1);
        } else {
            // Futility pruning. Note: it would be nice to do extensions and
            // futility before calling do_move, but this would require more
            // efficient ways of identifying important moves without actually
            // making them.
            bool prune_futile = futility_enabled &&
                !full_window &&
                !ext &&
                !mate_threat &&
                depth <= FUTILITY_DEPTH_LIMIT &&
                !get_move_capture(move) &&
                !get_move_promote(move);
            if (prune_futile) {
                // History pruning.
                if (history_prune_enabled && is_history_prune_allowed(
                            &root_data.history, move, depth)) {
                    num_futile_moves++;
                    undo_move(pos, move, &undo);
                    continue;
                }
                // Value pruning.
                if (value_prune_enabled && lazy_score < beta) {
                    if (futility_score == mated_in(-1)) {
                        futility_score = full_eval(pos) +
                            futility_margin[depth-1];
                    }
                    if (futility_score < beta) {
                        num_futile_moves++;
                        undo_move(pos, move, &undo);
                        continue;
                    }
                }
            }
            // Late move reduction (LMR), as described by Tord Romstad at
            // http://www.glaurungchess.com/lmr.html
            const bool move_is_late = full_window ?
                num_legal_moves > LMR_PV_EARLY_MOVES :
                num_legal_moves > LMR_EARLY_MOVES;
            const bool do_lmr = lmr_enabled &&
                move_is_late &&
                !ext &&
                !mate_threat &&
                depth > LMR_DEPTH_LIMIT &&
                get_move_promote(move) != QUEEN &&
                !get_move_capture(move) &&
                !is_move_castle(move);
            if (do_lmr) {
                score = -search(pos, search_node+1, ply+1,
                        -alpha-1, -alpha, depth-LMR_REDUCTION-1);
            } else {
                score = alpha+1;
            }
            if (score > alpha) {
                score = -search(pos, search_node+1, ply+1,
                        -alpha-1, -alpha, depth+ext-1);
                if (score > alpha) {
                    score = -search(pos, search_node+1, ply+1,
                            -beta, -alpha, depth+ext-1);
                }
            }
        }
        searched_moves[num_searched_moves++] = move;
        undo_move(pos, move, &undo);
        if (score > alpha) {
            alpha = score;
            update_pv(search_node->pv, (search_node+1)->pv, ply, move);
            check_line(pos, search_node->pv+ply);
            if (score >= beta) {
                if (!get_move_capture(move) &&
                        !get_move_promote(move)) {
                    record_success(&root_data.history, move, depth);
                    for (int i=0; i<num_searched_moves; ++i) {
                        move_t m = searched_moves[i];
                        if (!get_move_capture(m) && !get_move_promote(m)) {
                            record_failure(&root_data.history, m);
                        }
                    }
                    if (move != search_node->killers[0]) {
                        search_node->killers[1] = search_node->killers[0];
                        search_node->killers[0] = move;
                    }
                }
                put_transposition(pos, move, depth, beta, SCORE_LOWERBOUND);
                root_data.stats.move_selection[
                    MIN(num_legal_moves-1, HIST_BUCKETS)]++;
                search_node->pv[ply] = NO_MOVE;
                return beta;
            }
        }
    }
    if (!num_legal_moves) {
        // No legal moves, this is either stalemate or checkmate.
        search_node->pv[ply] = NO_MOVE;
        if (is_check(pos)) return mated_in(ply);
        return DRAW_VALUE;
    }

    root_data.stats.move_selection[MIN(num_legal_moves-1, HIST_BUCKETS)]++;
    if (alpha == orig_alpha) {
        put_transposition(pos, NO_MOVE, depth, alpha, SCORE_UPPERBOUND);
    } else {
        put_transposition(pos, search_node->pv[ply], depth, alpha, SCORE_EXACT);
    }
    return alpha;
}

/*
 * Search a position until it becomes "quiet". This is called at the leaves
 * of |search| to avoid using the static evaluator on positions that have
 * easy tactics on the board.
 */
static int quiesce(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth)
{
    search_node->pv[ply] = NO_MOVE;
    if (root_data.engine_status == ENGINE_ABORTED) return 0;
    if (alpha > mate_in(ply-1)) return alpha; // can't beat this
    if (is_draw(pos)) return DRAW_VALUE;

    int eval = full_eval(pos);
    int score = eval;
    if (ply >= MAX_SEARCH_DEPTH-1) return score;
    open_qnode(&root_data, ply);
    move_list_t move_list;
    move_t* moves = move_list.moves;
    int* scores = move_list.scores;
    if (is_check(pos)) {
        int evasions = generate_evasions(pos, moves);
        if (!evasions) return mated_in(ply);
    } else {
        if (alpha < score) alpha = score;
        if (alpha >= beta) return beta;
        if (!generate_quiescence_moves(pos, moves, depth == 0)) return alpha;
    }
    
    bool full_window = (beta-alpha > 1);
    bool allow_futility = qfutility_enabled &&
        !full_window &&
        !is_check(pos) &&
        pos->num_pieces[pos->side_to_move] > 2;
    order_moves(pos, search_node, &move_list, NO_MOVE, ply);
    int move_index = 0;
    for (move_t move = pick_move(&move_list, move_index<4); move != NO_MOVE;
            move = pick_move(&move_list, move_index<4), ++move_index) {
        check_pseudo_move_legality(pos, move);
        if (!is_pseudo_move_legal(pos, move)) continue;
        if (!is_check(pos) &&
                (!get_move_promote(move) || get_move_promote(move) != QUEEN) &&
                scores[move_index] < MAX_HISTORY) continue;
        // TODO: prevent futility for passed pawn moves and checks
        if (allow_futility &&
                get_move_promote(move) != QUEEN &&
                eval + material_value(pos->board[get_move_to(move)]) +
                qfutility_margin < alpha) continue;
        undo_info_t undo;
        do_move(pos, move, &undo);
        score = -quiesce(pos, search_node+1, ply+1, -beta, -alpha, depth-1);
        undo_move(pos, move, &undo);
        if (score > alpha) {
            alpha = score;
            update_pv(search_node->pv, (search_node+1)->pv, ply, move);
            check_line(pos, search_node->pv+ply);
            if (score >= beta) {
                return beta;
            }
        }
    }
    return alpha;
}

