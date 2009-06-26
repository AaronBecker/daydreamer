
#include <stdio.h>
#include "grasshopper.h"

static uint64_t full_search(position_t* pos, int depth);

void perft(char* fen_initial_position, int depth)
{
    position_t position;
    set_position(&position, fen_initial_position);
    print_board(&position);
    for (int d=1; d<=depth; ++d) {
        uint64_t nodes = full_search(&position, d);
        printf("depth %2d: %15llu\n", d, nodes);
    }
}

static uint64_t full_search(position_t* pos, int depth)
{
    move_t move_list[256];
    move_t* current_move = move_list;
    int move_count = generate_moves(pos, move_list);
    if (depth == 1) {
        return move_count;
    }

    uint64_t nodes = 0;
    while(*current_move) {
        undo_info_t undo;
        do_move(pos, *current_move, &undo);
        nodes += full_search(pos, depth-1);
        undo_move(pos, *current_move, &undo);
        ++current_move;
    }
    return nodes;
}
