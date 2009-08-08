
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "daydreamer.h"

search_data_t root_data;

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
        printf("%.2f ", (float)search_data->stats.move_selection[i]/total_moves);
    }
    printf("\n");
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
        check_for_input(data);
    }
    data->search_stack[ply].killers[0] = NO_MOVE;
    data->search_stack[ply].killers[1] = NO_MOVE;
}

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
    // the next iteration anyway. TODO: this margin could be tightened up.
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

    move_t id_pv[MAX_SEARCH_DEPTH];
    id_pv[0] = NO_MOVE;
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
        memcpy(id_pv, search_data->pv, MAX_SEARCH_DEPTH * sizeof(int));
        id_score = search_data->best_score;
        if (!should_deepen(search_data)) {
            ++search_data->current_depth;
            break;
        }
        print_pv(search_data);
    }
    stop_timer(&search_data->timer);

    --search_data->current_depth;
    search_data->best_score = id_score;
    memcpy(search_data->pv, id_pv, MAX_SEARCH_DEPTH * sizeof(int));
    print_pv(search_data);
    print_search_stats(search_data);
    printf("info string targettime %d elapsedtime %d\n",
            search_data->time_target, elapsed_time(&search_data->timer));
    print_transposition_stats();
    char la_move[6];
    move_to_la_str(search_data->best_move, la_move);
    printf("bestmove %s\n", la_move);
    search_data->engine_status = ENGINE_IDLE;
}

/*
 * Perform search at the root position. |search_data| contains all relevant
 * search information, which is set in |deepening_search|.
 */
static bool root_search(search_data_t* search_data)
{
    int alpha = -MATE_VALUE-1, beta = MATE_VALUE+1;
    search_data->best_score = -MATE_VALUE-1;
    position_t* pos = &search_data->root_pos;
    move_t hash_move = NO_MOVE;
    get_transposition(pos, search_data->current_depth,
            &alpha, &beta, &hash_move);
    order_moves(&search_data->root_pos, search_data->search_stack,
            search_data->root_moves, hash_move, 0);
    for (move_t* move=search_data->root_moves; *move; ++move) {
        if (should_output(search_data)) {
            char la_move[6];
            move_to_la_str(*move, la_move);
            printf("info currmove %s currmovenumber %d\n",
                    la_move, move - search_data->root_moves);
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
                    char la_move[6];
                    move_to_la_str(*move, la_move);
                    printf("info string fail high, research %s\n", la_move);
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
            if (should_output(search_data)) {
                print_pv(search_data);
            }
        }
    }
    if (alpha != -MATE_VALUE-1) {
        put_transposition(pos, search_data->best_move,
                search_data->current_depth,
                search_data->best_score, SCORE_EXACT);
    }
    return true;
}

static int search(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth)
{
    search_node->pv[ply] = NO_MOVE;
    if (root_data.engine_status == ENGINE_ABORTED) return 0;
    if (alpha > MATE_VALUE - ply - 1) return alpha; // can't beat this
    if (depth <= 0) {
        return quiesce(pos, search_node, ply, alpha, beta, depth);
    }
    if (is_draw(pos)) return DRAW_VALUE;
    open_node(&root_data, ply);
    bool full_window = (beta-alpha > 1);
    int orig_alpha = alpha;
    move_t hash_move = NO_MOVE;
    if (full_window) {
        // Get hash move, but keep full alpha-beta window.
        int a=0, b=0;
        get_transposition(pos, depth, &a, &b, &hash_move);
    } else {
        // Get hash move and let the table update alpha and beta. If the result
        // is good enough, we're done.
        if (get_transposition(pos, depth, &alpha, &beta, &hash_move) &&
                alpha >= beta) {
            search_node->pv[ply] = hash_move;
            search_node->pv[ply+1] = NO_MOVE;
            root_data.stats.cutoffs[root_data.current_depth]++;
            return alpha;
        }
    }

    int score = -MATE_VALUE-1;
    // nullmove reduction, just check for beta cutoff
    // TODO: factor into its own function
    if (is_nullmove_allowed(pos)) {
        undo_info_t undo;
        do_nullmove(pos, &undo);
        score = -search(pos, search_node+1, ply+1,
                -beta, -beta+1, MAX(depth-NULL_R, 0));
        undo_nullmove(pos, &undo);
        if (score >= beta) {
            int rdepth = depth - NULLMOVE_DEPTH_REDUCTION;
            if (rdepth > 0) {
                score = search(pos, search_node, ply, alpha, beta, rdepth);
                if (score >= beta) return beta;
            } else {
                return beta;
            }
        }
    }

    if (hash_move == NO_MOVE && is_iid_allowed(full_window, depth)) {
        const int iid_depth = depth -
            (full_window ? root_data.options.iid_pv_depth_reduction :
                           root_data.options.iid_non_pv_depth_reduction);
        assert(iid_depth > 0);
        search(pos, search_node, ply, alpha, beta, iid_depth);
        hash_move = search_node->pv[ply];
    }

    move_t moves[256];
    generate_pseudo_moves(pos, moves);
    int num_legal_moves = 0;
    order_moves(pos, search_node, moves, hash_move, ply);
    for (move_t* move = moves; *move; ++move) {
        if (!is_move_legal(pos, *move)) continue;
        ++num_legal_moves;
        // TODO: late move reductions
        undo_info_t undo;
        do_move(pos, *move, &undo);
        int ext = extend(pos, *move);
        if (move == moves) {
            // First move, use full window search.
            score = -search(pos, search_node+1, ply+1,
                    -beta, -alpha, depth+ext-1);
        } else {
            score = -search(pos, search_node+1, ply+1,
                    -alpha-1, -alpha, depth+ext-1);
            if (score > alpha) {
                score = -search(pos, search_node+1, ply+1,
                        -beta, -alpha, depth+ext-1);
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
    score_type_t score_type = (alpha == orig_alpha) ?
        SCORE_UPPERBOUND : SCORE_EXACT;
    put_transposition(pos, search_node->pv[ply], depth, alpha, score_type);
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
    int eval = simple_eval(pos);
    int score = eval;
    if (ply >= MAX_SEARCH_DEPTH-1) return score;
    if (!is_check(pos) && alpha < score) {
        alpha = score;
    }
    if (alpha >= beta) return beta;
    
    move_t moves[256];
    if (!generate_pseudo_captures(pos, moves)) return alpha;
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

/*
 * Basic minimax search, no pruning or cleverness of any kind. Used strictly
 * for debugging.
 */
static int minimax(position_t* pos,
        search_node_t* search_node,
        int ply,
        int depth)
{
    if (root_data.engine_status == ENGINE_ABORTED) return 0;
    if (!depth) {
        search_node->pv[ply] = NO_MOVE;
        return simple_eval(pos);
    }
    open_node(&root_data, ply);
    int score, best_score = -MATE_VALUE-1;
    move_t moves[256];
    int num_moves = generate_legal_moves(pos, moves);
    for (move_t* move = moves; *move; ++move) {
        undo_info_t undo;
        do_move(pos, *move, &undo);
        score = -minimax(pos, search_node+1, ply+1, depth-1);
        undo_move(pos, *move, &undo);
        if (score > best_score) {
            best_score = score;
            update_pv(search_node->pv, (search_node+1)->pv, ply, *move);
            check_line(pos, search_node->pv);
        }
    }
    if (!num_moves) {
        // No legal moves, this is either stalemate or checkmate.
        search_node->pv[ply] = NO_MOVE;
        // note: adjust MATE_VALUE by ply so that we favor shorter mates
        if (is_check(pos)) {
            return -(MATE_VALUE-ply);
        }
        return DRAW_VALUE;
    }
    return best_score;
}

/*
 * Full minimax tree search. As slow and accurate as possible. Used to debug
 * more sophisticated search strategies.
 */
void root_search_minimax(void)
{
    position_t* pos = &root_data.root_pos;
    int depth = root_data.depth_limit;
    root_data.engine_status = ENGINE_THINKING;
    init_timer(&root_data.timer);
    start_timer(&root_data.timer);
    if (!*root_data.root_moves) generate_legal_moves(pos, root_data.root_moves);

    root_data.best_score = -MATE_VALUE-1;
    for (move_t* move=root_data.root_moves; *move; ++move) {
        undo_info_t undo;
        do_move(pos, *move, &undo);
        int score = -minimax(pos, root_data.search_stack, 1, depth-1);
        undo_move(pos, *move, &undo);
        // update score
        if (score > root_data.best_score) {
            root_data.best_score = score;
            root_data.best_move = *move;
            update_pv(root_data.pv, root_data.search_stack->pv, 0, *move);
            check_line(pos, root_data.pv);
            print_pv(&root_data);
        }
    }
        
    stop_timer(&root_data.timer);
    printf("info string targettime %d elapsedtime %d\n",
            root_data.time_target, elapsed_time(&root_data.timer));
    print_pv(&root_data);
    char la_move[6];
    move_to_la_str(root_data.best_move, la_move);
    printf("bestmove %s\n", la_move);
    root_data.engine_status = ENGINE_IDLE;
}

