
#include <stdio.h>
#include "grasshopper.h"

static uint64_t full_search(position_t* pos, int depth, int target_depth);

void perft(char* fen_initial_position, int depth)
{
    position_t position;
    set_position(&position, fen_initial_position);
    print_board(&position);
    full_search(&position, depth, depth);
}

static uint64_t full_search(position_t* pos, int depth, int target_depth)
{
    move_t move_list[256];
    move_t* current_move = move_list;
    generate_moves(pos, move_list);
    if (depth == 0) {
        return 1;
    }

    uint64_t nodes = 0;
    while(*current_move) {
        undo_info_t undo;
        char la_move[6];
        move_to_la_str(*current_move, la_move);
        printf("depth %d, searching move %s\n", target_depth-depth, la_move);
        print_board(pos);
        do_move(pos, *current_move, &undo);
        nodes += full_search(pos, depth-1, target_depth);
        undo_move(pos, *current_move, &undo);
        ++current_move;
    }
    printf("depth %d: %llu\n", target_depth-depth, nodes);
    return nodes;
}
