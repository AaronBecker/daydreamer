
#include <stdio.h>
#include <strings.h>
#include "grasshopper.h"

#define MATE_VALUE  0x10000
#define DRAW_VALUE  0

int search(position_t* pos, int alpha, int beta, int depth)
{
    if (!depth) {
        int score = simple_eval(pos);
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
        if (pv) score = -search(pos, -beta, -alpha, depth-1);
        else {
            score = -search(pos, -alpha-1, -alpha, depth-1);
            if (score > alpha) score = -search(pos, -beta, -alpha, depth-1);
        }
        undo_move(pos, *move, &undo);
        if (score >= beta) return beta;
        if (score > alpha) {
            alpha = score;
            pv = false;
        }
    }
    return alpha;
}

void root_search(position_t* pos, int depth)
{
    move_t root_moves[256];
    int scores[256];
    bzero(scores, 256*sizeof(int));
    generate_legal_moves(pos, root_moves);
    int alpha = -MATE_VALUE-1, beta = MATE_VALUE+1;
    int score_index=0;
    for (move_t* move = root_moves; *move; ++move, ++score_index) {
        undo_info_t undo;
        do_move(pos, *move, &undo);
        scores[score_index] = -search(pos, -beta, -alpha, depth-1);
        undo_move(pos, *move, &undo);
    }
    
    score_index = 0;
    move_t best_move;
    int best_score=alpha;
    char la_move[6];
    for (move_t* move = root_moves; *move; ++move, ++score_index) {
        move_to_la_str(*move, la_move);
        printf("%s:\t%d\n", la_move, scores[score_index]);
        if (scores[score_index] > best_score) {
            best_score = scores[score_index];
            best_move = *move;
        }
    }
    move_to_la_str(best_move, la_move);
    printf("best move: %s, %d\n", la_move, best_score);

    // TODO: iterative deepening
    //      TODO: sort moves based on prev iteration
}
