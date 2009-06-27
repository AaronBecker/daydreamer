
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "grasshopper.h"

static uint64_t full_search(position_t* pos, int depth);
static uint64_t divide(position_t* pos, int depth);
extern int errno;

void perft_testsuite(char* filename)
{
    char test_storage[1024];
    char* test = test_storage;
    position_t pos;
    timer_t perft_timer;
    reset_timer(&perft_timer);
    FILE* test_file = fopen(filename, "r");
    if (!test_file) {
        printf("Couldn't open perft test file %s: %s\n",
                filename, strerror(errno));
        return;
    }
    int total_tests = 0, correct_tests = 0;
    while (fgets(test, 1024, test_file)) {
        char* fen = strsep(&test, ";");
        set_position(&pos, fen);
        printf("Test %d: %s\n", total_tests+1, fen);
        bool failure = false;
        do {
            int depth;
            uint64_t correct_answer;
            sscanf(test, "D%d %llu", &depth, &correct_answer);
            resume_timer(&perft_timer);
            uint64_t result = full_search(&pos, depth);
            int time = stop_timer(&perft_timer);
            printf("\tDepth %d: %15llu", depth, result);
            if (result != correct_answer) {
                failure = true;
                printf(" expected %15llu -- FAIL",
                        correct_answer);
            } else printf(" -- SUCCESS");
            printf(" / %.2fs\n", time/1000.0);
        } while ((test = strchr(test, ';') + 1) != (char*)1);
        ++total_tests;
        if (!failure) ++correct_tests;
        test = test_storage;
    }
    printf("Tests completed. %d/%d tests passed in %.2fs.\n",
            correct_tests, total_tests, elapsed_time(&perft_timer)/1000.0);
}

uint64_t perft(position_t* position, int depth, bool div)
{
    timer_t perft_timer;
    start_timer(&perft_timer);
    uint64_t nodes;
    if (div) {
        nodes = divide(position, depth);
    } else {
        nodes = full_search(position, depth);
        printf("depth %d: %llu, ", depth, nodes);
    }
    stop_timer(&perft_timer);
    printf("elapsed time %d ms\n", elapsed_time(&perft_timer));
    return nodes;
}

static uint64_t divide(position_t* pos, int depth)
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
    return total_nodes;
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
