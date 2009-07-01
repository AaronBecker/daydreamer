
#include <stdio.h>
#include <strings.h>
#include "grasshopper.h"

#define CHECK_INTERVAL  0xffff
#define MATE_VALUE      0xfff
#define DRAW_VALUE      0

search_data_t root_data;

static int quiesce(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth);


void init_search_data(void)
{
    position_t copy;
    copy_position(&copy, &root_data.root_pos);
    memset(&root_data, 0, sizeof(root_data));
    copy_position(&root_data.root_pos, &copy);
    init_timer(&root_data.timer);
}

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

void perform_periodic_checks(search_data_t* data)
{
    if (should_stop_searching(data)) data->engine_status = ENGINE_ABORTED;
    check_for_input(data);
}

int search(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth)
{
    if (root_data.engine_status == ENGINE_ABORTED) return 0;
    if (alpha > MATE_VALUE - ply - 1) return alpha; // can't beat this
    if (!depth) {
        //search_node->pv[ply] = NO_MOVE;
        //return simple_eval(pos);
        return quiesce(pos, search_node, ply, alpha, beta, depth);
    }
    if (++root_data.nodes_searched & CHECK_INTERVAL) {
        perform_periodic_checks(&root_data);
    }
    bool pv = true;
    int score;
    move_t moves[256];
    generate_pseudo_moves(pos, moves);
    int num_legal_moves = 0;
    for (move_t* move = moves; *move; ++move) {
        if (!is_move_legal(pos, *move)) continue;
        ++num_legal_moves;
        undo_info_t undo;
        do_move(pos, *move, &undo);
        if (pv) score = -search(pos, search_node+1, ply+1,
                -beta, -alpha, depth-1);
        else {
            score = -search(pos, search_node+1, ply+1,
                    -alpha-1, -alpha, depth-1);
            if (score > alpha) score = -search(pos, search_node+1, ply+1,
                    -beta, -alpha, depth-1);
        }
        undo_move(pos, *move, &undo);
        if (score >= beta) return beta;
        if (score > alpha) {
            alpha = score;
            pv = false;
            // update pv from child search nodes
            search_node->pv[ply] = *move;
            int i = ply;
            do {
                ++i;
                search_node->pv[i] = (search_node+1)->pv[i];
            } while ((search_node+1)->pv[i] != NO_MOVE);
        }
    }
    if (!num_legal_moves) {
        // No legal moves, this is either stalemate or checkmate.
        search_node->pv[ply] = NO_MOVE;
        // note: adjust MATE_VALUE by ply so that we favor shorter mates
        if (is_check(pos)) {
            return -(MATE_VALUE-ply);
        }
        return DRAW_VALUE;
    }
    return alpha;
}

void root_search(void)
{
    position_t* pos = &root_data.root_pos;
    root_data.engine_status = ENGINE_THINKING;
    init_timer(&root_data.timer);
    start_timer(&root_data.timer);
    if (!*root_data.root_moves) generate_legal_moves(pos, root_data.root_moves);

    // iterative deepening loop
    int move_index = 0, best_index=0;
    int* curr_depth = &root_data.current_depth;
    for (*curr_depth=1;
            !root_data.depth_limit || *curr_depth<=root_data.depth_limit;
            ++*curr_depth) {
        int alpha = -MATE_VALUE-1, beta = MATE_VALUE+1;
        root_data.best_score = -MATE_VALUE-1;
        printf("info depth %d\n", *curr_depth);
        for (move_t* move=root_data.root_moves; *move; ++move, ++move_index) {
            undo_info_t undo;
            do_move(pos, *move, &undo);
            int score = -search(pos, root_data.search_stack, 1,
                    -beta, -alpha, *curr_depth-1);
            undo_move(pos, *move, &undo);
            // update score
            if (score > alpha) {
                alpha = score;
                if (score > root_data.best_score) {
                    root_data.best_score = score;
                    root_data.best_move = *move;
                    best_index = move_index;
                    // update pv
                    root_data.pv[0] = *move;
                    int i=1;
                    for (; root_data.search_stack->pv[i] != NO_MOVE; ++i) {
                        root_data.pv[i] = root_data.search_stack->pv[i];
                    }
                    root_data.pv[i] = NO_MOVE;
                    print_pv(root_data.pv, *curr_depth,
                            root_data.best_score,
                            elapsed_time(&root_data.timer),
                            root_data.nodes_searched);
                    // TODO: improve move ordering 
                }
            }
            // move the pv move to the front of the list
            root_data.root_moves[best_index] = root_data.root_moves[0];
            root_data.root_moves[0] = root_data.best_move;
        }
        
        if (!should_deepen(&root_data)) break;
    }
    stop_timer(&root_data.timer);
    printf("info string targettime %d elapsedtime %d\n",
            root_data.time_target, elapsed_time(&root_data.timer));
    
    print_pv(root_data.pv, root_data.current_depth,
            root_data.best_score,
            elapsed_time(&root_data.timer),
            root_data.nodes_searched);
    char la_move[6];
    move_to_la_str(root_data.best_move, la_move);
    printf("bestmove %s\n", la_move);
    root_data.engine_status = ENGINE_IDLE;
}

static int quiesce(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth)
{
    if (root_data.engine_status == ENGINE_ABORTED) return 0;
    if (++root_data.nodes_searched & CHECK_INTERVAL) {
        perform_periodic_checks(&root_data);
    }
    if (alpha > MATE_VALUE - ply - 1) return alpha; // can't beat this
    int eval = simple_eval(pos);
    int score = eval;
    if (score >= beta) return beta;
    if (alpha < score) alpha = score;
    
    move_t moves[256];
    generate_pseudo_captures(pos, moves);
    int num_legal_captures = 0;
    for (move_t* move = moves; *move; ++move) {
        if (static_exchange_eval(pos, *move) < 0) continue;
        if (!is_move_legal(pos, *move)) continue;
        ++num_legal_captures;
        undo_info_t undo;
        do_move(pos, *move, &undo);
        score = -quiesce(pos, search_node+1, ply+1, -beta, -alpha, depth-1);
        undo_move(pos, *move, &undo);
        if (score >= beta) return beta;
        if (score > alpha) {
            alpha = score;
            // update pv from child search nodes
            search_node->pv[ply] = *move;
            int i = ply;
            do {
                ++i;
                search_node->pv[i] = (search_node+1)->pv[i];
            } while ((search_node+1)->pv[i] != NO_MOVE);
        }
    }
    if (!num_legal_captures) {
        search_node->pv[ply] = NO_MOVE;
        int num_legal_noncaptures = generate_legal_noncaptures(pos, moves);
        if (num_legal_noncaptures) {
            // we've reached quiescence
            return eval;
        }
        // No legal moves, this is either stalemate or checkmate.
        // note: adjust MATE_VALUE by ply so that we favor shorter mates
        if (is_check(pos)) {
            return -(MATE_VALUE-ply);
        }
        return DRAW_VALUE;
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
    if (++root_data.nodes_searched & CHECK_INTERVAL) {
        perform_periodic_checks(&root_data);
    }
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
            // update pv from child search nodes
            search_node->pv[ply] = *move;
            int i = ply;
            do {
                ++i;
                search_node->pv[i] = (search_node+1)->pv[i];
            } while ((search_node+1)->pv[i] != NO_MOVE);
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
            // update pv
            root_data.pv[0] = *move;
            int i=1;
            for (; root_data.search_stack->pv[i] != NO_MOVE; ++i) {
                root_data.pv[i] = root_data.search_stack->pv[i];
            }
            root_data.pv[i] = NO_MOVE;
            print_pv(root_data.pv, depth,
                    root_data.best_score,
                    elapsed_time(&root_data.timer),
                    root_data.nodes_searched);
        }
    }
        
    stop_timer(&root_data.timer);
    printf("info string targettime %d elapsedtime %d\n",
            root_data.time_target, elapsed_time(&root_data.timer));
    
    print_pv(root_data.pv, depth,
            root_data.best_score,
            elapsed_time(&root_data.timer),
            root_data.nodes_searched);
    char la_move[6];
    move_to_la_str(root_data.best_move, la_move);
    printf("bestmove %s\n", la_move);
    root_data.engine_status = ENGINE_IDLE;
}


