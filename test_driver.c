
#include <strings.h>
#include <stdio.h>
#include "grasshopper.h"

int main()
{
    char* fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    position_t pos;
    move_t moves[100];
    bzero(moves, 100*sizeof(move_t));
    set_position(&pos, fen);
    print_board(&pos);
    generate_moves(&pos, moves);
    print_la_move_list(moves);
    fen = "r2q1rk1/pp2ppbp/1np2np1/2Q3B1/3PP1b1/2N2N2/PP3PPP/3RKB1R";
    set_position(&pos, fen);
    bzero(moves, 100*sizeof(move_t));
    print_board(&pos);
    generate_moves(&pos, moves);
    print_la_move_list(moves);
}
