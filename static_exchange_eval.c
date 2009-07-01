
#include "grasshopper.h"

int static_exchange_eval(move_t move)
{
    square_t from = get_move_from(move);
    square_t to = get_move_to(move);
    piece_t attacker = get_move_piece(move);
    assert(get_move_capture(move));
    piece_t attackers[2][16];
    int num_attackers[2];

}
