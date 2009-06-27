
#include <assert.h>
#include <stdio.h>
#include "grasshopper.h"

static uint64_t full_search(position_t* pos, int depth);
static void divide(position_t* pos, int depth);

void perft_fen(char* fen_initial_position, int depth, bool div)
{
    position_t position;
    set_position(&position, fen_initial_position);
    perft(&position, depth, div);
}

void perft(position_t* position, int depth, bool div)
{
    print_board(position);
    if (div) {
        divide(position, depth);
    } else {
        for (int d=1; d<=depth; ++d) {
                uint64_t nodes = full_search(position, d);
                printf("depth %2d: %15llu\n", d, nodes);
        }
    }
}

static void divide(position_t* pos, int depth)
{
    move_t move_list[256];
    move_t* current_move = move_list;
    int num_moves = generate_legal_moves(pos, move_list);

    uint64_t child_nodes, total_nodes=0;
    char la_move[6];
    while (*current_move) {
        undo_info_t undo;
        do_move(pos, *current_move, &undo);
        child_nodes = full_search(pos, depth-1);
        total_nodes += child_nodes;
        undo_move(pos, *current_move, &undo);
        move_to_la_str(*current_move, la_move);
        printf("%s: %llu\n", la_move, child_nodes);
        ++current_move;
    }
    printf("%d moves, %llu total nodes\n", num_moves, total_nodes);
}

static uint64_t full_search(position_t* pos, int depth)
{
    if (depth <= 0) return 1;
    move_t move_list[256];
    move_t* current_move = move_list;
    generate_moves(pos, move_list);
    int move_count = generate_legal_moves(pos, move_list);
    if (depth == 1) return move_count;

    uint64_t nodes = 0;
    while (*current_move) {
        undo_info_t undo;
        do_move(pos, *current_move, &undo);
        nodes += full_search(pos, depth-1);
        undo_move(pos, *current_move, &undo);
        ++current_move;
    }
    return nodes;
}
