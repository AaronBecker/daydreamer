
#include <strings.h>
#include <stdio.h>
#include "grasshopper.h"

int main(void)
{
    char* fen = "rnbqkbnr/pppppppp/8/8/8/N7/PPPPPPPP/R1BQKBNR b KQkq - 0 1";
    position_t pos;
    move_t moves[100];
    bzero(moves, 100*sizeof(move_t));
    set_position(&pos, fen);
    print_board(&pos);
    generate_moves(&pos, moves);
    print_la_move_list(moves);
    perft("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3);
}
