
#include <stdio.h>
#include <strings.h>
#include "grasshopper.h"

#define CHECK_INTERVAL  0xffff
#define MATE_VALUE      0xfff
#define DRAW_VALUE      0

search_data_t root_data;

void init_search_data(void)
{
    memset(&root_data, 0, sizeof(root_data));
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
    if (data->depth_limit &&
            data->current_depth >= data->depth_limit) return true;
    return false;
}

void perform_periodic_checks(search_data_t* data)
{
    if (should_stop_searching(data)) data->engine_status = ENGINE_ABORTED;
    check_for_input(data);
    //printf("currtime %d targettime %d\n", elapsed_time(&data->timer), data->time_target);
}

int search(position_t* pos,
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
    if (!depth) {
        int score = simple_eval(pos);
        search_node->pv[ply] = NO_MOVE;
        return score; // TODO: quiescence search
    }
    bool pv = true;
    int score;
    move_t moves[256];
    generate_pseudo_moves(pos, moves);
    for (move_t* move = moves; *move; ++move) {
        if (!is_move_legal(pos, *move)) continue;
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
    return alpha;
}

void root_search(void)
{
    position_t* pos = &root_data.root_pos;
    root_data.engine_status = ENGINE_THINKING;
    init_timer(&root_data.timer);
    start_timer(&root_data.timer);
    generate_legal_moves(pos, root_data.root_moves);
    int alpha = -MATE_VALUE-1, beta = MATE_VALUE+1;

    // iterative deepening loop
    int move_index = 0;
    int* curr_depth = &root_data.current_depth;
    for (*curr_depth=1;
            !root_data.depth_limit || *curr_depth<=root_data.depth_limit;
            ++*curr_depth) {
        for (move_t* move=root_data.root_moves; *move; ++move, ++move_index) {
            undo_info_t undo;
            do_move(pos, *move, &undo);
            root_data.move_scores[move_index] = -search(pos,
                    root_data.search_stack, 1, -beta, -alpha, *curr_depth-1);
            int i=1;
            root_data.pvs[move_index][0] = *move;
            for (; root_data.search_stack->pv[i] != NO_MOVE; ++i) {
                root_data.pvs[move_index][i] = root_data.search_stack->pv[i];
            }
            root_data.pvs[move_index][i] = NO_MOVE;
            undo_move(pos, *move, &undo);
            print_pv(root_data.pvs[move_index], *curr_depth,
                    root_data.move_scores[move_index],
                    elapsed_time(&root_data.timer),
                    root_data.nodes_searched);
        }
        
        if (should_stop_searching(&root_data)) break;
        // TODO: sort moves based on scores
        // TODO: figure out when the current result is "good enough",
        // regardless of search params, and stop if possible.
    }
    stop_timer(&root_data.timer);
    printf("info string targettime %d elapsedtime %d\n",
            root_data.time_target, elapsed_time(&root_data.timer));
    
    move_index = 0;
    move_t best_move;
    int best_score=alpha;
    int best_index=-1;
    char la_move[6];
    for (move_t* move = root_data.root_moves; *move; ++move, ++move_index) {
        if (root_data.move_scores[move_index] > best_score) {
            best_index = move_index;
            best_score = root_data.move_scores[move_index];
            best_move = *move;
        }
    }
    print_pv(root_data.pvs[best_index], *curr_depth,
            root_data.move_scores[best_index],
            elapsed_time(&root_data.timer),
            root_data.nodes_searched);
    move_to_la_str(best_move, la_move);
    printf("bestmove %s\n", la_move);
}

