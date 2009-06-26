
#include <strings.h>
#include <stdio.h>
#include "grasshopper.h"

int main(void)
{
    //char* fen = "rnbqkbnr/pppppppp/8/8/8/N7/PPPPPPPP/R1BQKBNR b KQkq - 0 1";
    //position_t pos;
    //move_t moves[100];
    //bzero(moves, 100*sizeof(move_t));
    //set_position(&pos, fen);
    //print_board(&pos);
    //generate_moves(&pos, moves);
    //print_la_move_list(moves);
    //perft("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6);
    /*
        1	20
        2	400
        3	8902
        4	197281
        5	4865609
        6	119060324
        7	3195901860
        8	84998978956
        9	2439530234167
        10	69352859712417
    */
    perft("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
            6,
            false);
    /*
        1	48
        2	2039
        3	97862
        4	4085603
        5	193690690
        6	8031647685
     */
}

