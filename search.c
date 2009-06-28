
#include <stdio.h>
#include <strings.h>
#include "grasshopper.h"

#define MATE_VALUE  0x10000
#define DRAW_VALUE  0

static search_data_t root_search_data;

int search(position_t* pos,
        search_node_t* search_node,
        int ply,
        int alpha,
        int beta,
        int depth)
{
    root_search_data.nodes_searched++;
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

void root_search(position_t* pos, int depth)
{
    move_t root_moves[256];
    int scores[256];
    bzero(scores, 256*sizeof(int));
    search_node_t search_stack[MAX_SEARCH_DEPTH];
    bzero(search_stack, MAX_SEARCH_DEPTH*sizeof(search_node_t));
    move_t pvs[256][MAX_SEARCH_DEPTH];
    bzero(pvs, MAX_SEARCH_DEPTH*256*sizeof(move_t));
    bzero(&root_search_data, sizeof(root_search_data));

    init_timer(&root_search_data.timer);
    start_timer(&root_search_data.timer);
    generate_legal_moves(pos, root_moves);
    int alpha = -MATE_VALUE-1, beta = MATE_VALUE+1;
    int move_index=0;
    for (move_t* move = root_moves; *move; ++move, ++move_index) {
        undo_info_t undo;
        do_move(pos, *move, &undo);
        scores[move_index] = -search(pos, search_stack, 0,
                -beta, -alpha, depth-1);
        int i=0;
        for (; search_stack->pv[i] != NO_MOVE; ++i) {
            pvs[move_index][i] = search_stack->pv[i];
        }
        pvs[move_index][i] = NO_MOVE;
        undo_move(pos, *move, &undo);
    }
    stop_timer(&root_search_data.timer);
    
    move_index = 0;
    move_t best_move;
    int best_score=alpha;
    int best_index=-1;
    char la_move[6];
    for (move_t* move = root_moves; *move; ++move, ++move_index) {
        move_to_la_str(*move, la_move);
        printf("%s:\t%d\n", la_move, scores[move_index]);
        print_la_move_list(pvs[move_index]);
        if (scores[move_index] > best_score) {
            best_index = move_index;
            best_score = scores[move_index];
            best_move = *move;
        }
    }
    move_to_la_str(best_move, la_move);
    float time_taken = ((float)elapsed_time(&root_search_data.timer))/100.0;
    printf("\nbest move: %s, %d\n", la_move, best_score);
    print_la_move_list(pvs[best_index]);
    printf("nodes searched: %llu\n", root_search_data.nodes_searched);
    printf("time elapsed: %.2f, %.2f nodes/s\n", time_taken,
            root_search_data.nodes_searched / time_taken);

    // TODO: iterative deepening
    //      TODO: sort moves based on prev iteration
}
