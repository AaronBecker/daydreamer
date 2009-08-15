
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "daydreamer.h"

search_data_t root_data;
static const bool nullmove_enabled = true;
static const bool iid_enabled = true;
static const bool razoring_enabled = true;
static const bool futility_enabled = true;
static const bool lmr_enabled = true;
static const int futility_margin[FUTILITY_DEPTH_LIMIT] = {
    125, 250, 300, 500, 900
};
static const int razor_attempt_margin[RAZOR_DEPTH_LIMIT] = {
    150, 300, 300
};
static const int razor_cutoff_margin[RAZOR_DEPTH_LIMIT] = {
    0, 50, 150
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
    memset(data->root_moves, 0, sizeof(move_t) * 256);
    data->best_move = NO_MOVE;
    data->best_score = 0;
    memset(data->pv, 0, sizeof(move_t) * MAX_SEARCH_DEPTH);
    memset(data->search_stack, 0, sizeof(search_node_t) * MAX_SEARCH_DEPTH);
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

static void print_search_stats(search_data_t* search_data)
{
    printf("info string cutoffs ");
    for (int i=0; i<=search_data->current_depth; ++i) {
        printf("%d ", search_data->stats.cutoffs[i]);
    }
    printf("\ninfo string move selection ");
    int total_moves = 0;
    for (int i=0; i<HIST_BUCKETS; ++i) {
        total_moves += search_data->stats.move_selection[i];
    }
    for (int i=0; i<HIST_BUCKETS; ++i) {
        printf("%.2f ", (float)search_data->stats.move_selection[i] /
                total_moves);
    }
    printf("\ninfo string razoring attempts/cutoffs by depth "
            "%d/%d %d/%d %d/%d\n",
        search_data->stats.razor_attempts[0],
        search_data->stats.razor_prunes[0],
        search_data->stats.razor_attempts[1],
        search_data->stats.razor_prunes[1],
        search_data->stats.razor_attempts[2],
        search_data->stats.razor_prunes[2]);
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
 * TODO: recapture extensions would be good. Also, fractional extensions,
 * and fractional plies in general.
 */
static int extend(position_t* pos, move_t move)
{
    if (is_check(pos)) return 1;
    square_t sq = get_move_to(move);
    if (piece_type(pos->board[sq]->piece) == PAWN &&
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
    int piece_value = pos->material_eval[pos->side_to_move] -
        material_value(WK) -
        material_value(WP)*pos->piece_count[pos->side_to_move][PAWN];
    return piece_value != 0;
}

/*
 * Can we do internal iterative deepening? Controlled by search
 * parameters.
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
 * Take an unordered list of pseudo-legal moves and order them according
 * to how good we think they'll be. This just identifies a few key classes
 * of moves and insertion sorts them into place.
 */
static void order_moves(position_t* pos,
        search_node_t* search_node,
        move_t* moves,
        move_t hash_move,
        int ply)
{
    const int hash_score = 1000;
    const int promote_score = 900;
    const int underpromote_score = -600;
    const int killer_score = 800;
    int scores[256];
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        int score = 0;
        if (move == hash_move) score = hash_score;
        else if (get_move_promote(move) == QUEEN) {
            score = promote_score;
        } else if (get_move_promote(move) != NONE) {
            score = underpromote_score;
        } else if (get_move_capture(move)) {
            score = static_exchange_eval(pos, move);
        } else if (move == search_node->killers[0]) {
            score = killer_score;
        } else if (move == search_node->killers[1]) {
            score = killer_score-1;
        } else if (ply >=2 && move == (search_node-1)->killers[0]) {
            score = killer_score-2;
        } else if (ply >=2 && move == (search_node-1)->killers[1]) {
            score = killer_score-3;
        }
        // Insert the score into the right place in the list. The list is never
        // long enough to requre an n log n algorithm.
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
    if (search_data->root_moves[0] == NO_MOVE) {
        generate_legal_moves(&search_data->root_pos, search_data->root_moves);
    }

    int id_score = root_data.best_score = -MATE_VALUE-1;
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
    print_pv(search_data);
    print_search_stats(search_data);
    printf("info string targettime %d elapsedtime %d\n",
            search_data->time_target, elapsed_time(&search_data->timer));
    print_transposition_stats();
    char coord_move[6];
    move_to_coord_str(search_data->best_move, coord_move);
    printf("bestmove %s\n", coord_move);
    search_data->engine_status = ENGINE_IDLE;
}

/*
 * Perform search at the root position. |search_data| contains all relevant
 * search information, which is set in |deepening_search|.
 * TODO: use an aspiration window.
 */
static bool root_search(search_data_t* search_data)
{
    int alpha = -MATE_VALUE-1, beta = MATE_VALUE+1;
    search_data->best_score = -MATE_VALUE-1;
    position_t* pos = &search_data->root_pos;
    transposition_entry_t* trans_entry = get_transposition(pos);
    move_t hash_move = trans_entry ? trans_entry->move : NO_MOVE;
    // TODO: sort by # of nodes expanded in previous iteration.
    order_moves(&search_data->root_pos, search_data->search_stack,
            search_data->root_moves, hash_move, 0);
    for (move_t* move=search_data->root_moves; *move; ++move) {
        if (should_output(search_data)) {
            char coord_move[6];
            move_to_coord_str(*move, coord_move);
            printf("info currmove %s currmovenumber %d\n",
                    coord_move, move - search_data->root_moves);
        }
        undo_info_t undo;
        do_move(pos, *move, &undo);
        int ext = extend(pos, *move);
        int score;
        if (move == search_data->root_moves) {
            // First move, use full window search.
            score = -search(pos, search_data->search_stack,
                    1, -beta, -alpha, search_data->current_depth+ext-1);
        } else {
            score = -search(pos, search_data->search_stack,
                    1, -alpha-1, -alpha, search_data->current_depth+ext-1);
            if (score > alpha) {
                if (should_output(search_data)) {
                    char coord_move[6];
                    move_to_coord_str(*move, coord_move);
                    printf("info string fail high, research %s\n", coord_move);
                }
                score = -search(pos, search_data->search_stack,
                        1, -beta, -alpha, search_data->current_depth+ext-1);
            }
        }
        undo_move(pos, *move, &undo);
        if (search_data->engine_status == ENGINE_ABORTED) return false;
        // update score
        if (score > alpha) {
            alpha = score;
            if (score > search_data->best_score) {
                search_data->best_score = score;
                search_data->best_move = *move;
            }
            update_pv(search_data->pv, search_data->search_stack->pv, 0, *move);
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
    if (alpha > MATE_VALUE - ply - 1) return alpha; // Can't beat this.../daydreamer_testing/shortwac.epd
    if (depth <= 0) {
        return quiesce(pos, search_node, ply, alpha, beta, depth);
    }
    open_node(&root_data, ply);
    if (is_draw(pos)) return DRAW_VALUE;
    bool full_window = (beta-alpha > 1);

    // Put some bounds on how good/bad this node could turn out to be.
    int orig_alpha = alpha;
    alpha = MAX(alpha, -(MATE_VALUE-ply));
    beta = MIN(beta, MATE_VALUE-ply+1);
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

    int score = -MATE_VALUE-1;
    int lazy_score = simple_eval(pos);
    // Nullmove reduction.
    if (nullmove_enabled &&
            depth != 1 &&
            !full_window &&
            pos->prev_move != NULL_MOVE &&
            lazy_score + NULL_EVAL_MARGIN > beta &&
            is_nullmove_allowed(pos)) {
        undo_info_t undo;
        do_nullmove(pos, &undo);
        int null_score = -search(pos, search_node+1, ply+1,
                -beta, -beta+1, MAX(depth-NULL_R, 0));
        undo_nullmove(pos, &undo);
        if (null_score >= beta) {
            int rdepth = depth - NULLMOVE_VERIFICATION_REDUCTION;
            if (rdepth <= 0) return beta;
            null_score = search(pos, search_node, ply, alpha, beta, rdepth);
            if (null_score >= beta) return beta;
        }
    } else if (razoring_enabled &&
            pos->prev_move != NULL_MOVE &&
            !full_window &&
            depth <= RAZOR_DEPTH_LIMIT &&
            hash_move == NO_MOVE &&
            beta < MATE_VALUE - MAX_SEARCH_DEPTH &&
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

    move_t moves[256];
    generate_pseudo_moves(pos, moves);
    int num_legal_moves = 0, num_futile_moves = 0;
    order_moves(pos, search_node, moves, hash_move, ply);
    for (move_t* move = moves; *move; ++move) {
        if (!is_move_legal(pos, *move)) continue;
        ++num_legal_moves;
        undo_info_t undo;
        do_move(pos, *move, &undo);
        int ext = extend(pos, *move);
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
                depth <= FUTILITY_DEPTH_LIMIT &&
                !get_move_capture(*move) &&
                !get_move_promote(*move);
            if (prune_futile) {
                score = full_eval(pos) + futility_margin[depth-1];
                if (score < alpha) {
                    num_futile_moves++;
                    undo_move(pos, *move, &undo);
                    continue;
                }
            }
            // Late move reduction (LMR), as described by Tord Romstad at
            // http://www.glaurungchess.com/lmr.html
            const bool move_is_late = full_window ?
                num_legal_moves - num_futile_moves > LMR_PV_EARLY_MOVES :
                num_legal_moves - num_futile_moves > LMR_EARLY_MOVES;
            const bool do_lmr = lmr_enabled &&
                move_is_late &&
                !ext &&
                depth > LMR_DEPTH_LIMIT &&
                get_move_promote(*move) != QUEEN &&
                !get_move_capture(*move) &&
                !is_move_castle(*move);
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
        undo_move(pos, *move, &undo);
        if (score > alpha) {
            alpha = score;
            update_pv(search_node->pv, (search_node+1)->pv, ply, *move);
            check_line(pos, search_node->pv+ply);
            if (score >= beta) {
                if (!get_move_capture(*move) &&
                        !get_move_promote(*move) &&
                        *move != search_node->killers[0]) {
                    search_node->killers[1] = search_node->killers[0];
                    search_node->killers[0] = *move;
                }
                put_transposition(pos, *move, depth, beta, SCORE_LOWERBOUND);
                root_data.stats.move_selection[MIN(move-moves, HIST_BUCKETS)]++;
                search_node->pv[ply] = NO_MOVE;
                return beta;
            }
        }
    }
    if (!num_legal_moves) {
        // No legal moves, this is either stalemate or checkmate.
        search_node->pv[ply] = NO_MOVE;
        if (is_check(pos)) {
            // note: adjust MATE_VALUE by ply so that we favor shorter mates
            return -(MATE_VALUE-ply);
        }
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
    if (alpha > MATE_VALUE - ply - 1) return alpha; // can't beat this
    open_qnode(&root_data, ply);
    if (is_draw(pos)) return DRAW_VALUE;
    int eval = full_eval(pos);
    int score = eval;
    if (ply >= MAX_SEARCH_DEPTH-1) return score;
    if (!is_check(pos) && alpha < score) {
        alpha = score;
    }
    if (alpha >= beta) return beta;
    
    move_t moves[256];
    //if (!generate_quiescence_moves(pos, moves, depth == 0)) return alpha;
    if (!generate_pseudo_captures(pos, moves)) return alpha;
    // TODO: generate more moves to search. Good candidates are checks that
    // don't lose material (up to a certain number of consecutive checks, to
    // prevent a runaway) and promotions to queen.
    order_moves(pos, search_node, moves, NO_MOVE, ply);
    int num_legal_captures = 0;
    for (move_t* move = moves; *move; ++move) {
        if (!is_move_legal(pos, *move)) continue;
        ++num_legal_captures;
        if (static_exchange_eval(pos, *move) < 0) continue;
        undo_info_t undo;
        do_move(pos, *move, &undo);
        score = -quiesce(pos, search_node+1, ply+1, -beta, -alpha, depth-1);
        undo_move(pos, *move, &undo);
        if (score > alpha) {
            alpha = score;
            update_pv(search_node->pv, (search_node+1)->pv, ply, *move);
            check_line(pos, search_node->pv+ply);
            if (score >= beta) {
                return beta;
            }
        }
    }
    return alpha;
}

