
#include <assert.h>
#include <ctype.h>
#include "grasshopper.h"

square_t parse_la_square(const char* alg_square)
{
    assert(tolower(alg_square[0]) >= 'a' && tolower(alg_square[0]) < 'i');
    assert(alg_square[1] > '0' && alg_square[1] < '9'); 
    // TODO: log warning instead.
    return create_square(tolower(alg_square[0])-'a', alg_square[1]-'1');
}

move_t parse_la_move(position_t* pos, const char* la_move)
{
    square_t from = parse_la_square(la_move);
    square_t to = parse_la_square(la_move + 2);
    piece_type_t promote_type = NONE;
    switch (*(la_move+4)) {
        case 'n': case 'N': promote_type = KNIGHT; break;
        case 'b': case 'B': promote_type = BISHOP; break;
        case 'r': case 'R': promote_type = ROOK; break;
        case 'q': case 'Q': promote_type = QUEEN; break;
    }
    move_t possible_moves[256];
    int num_moves = generate_legal_moves(pos, possible_moves);
    move_t move;
    for (int i=0; i<num_moves; ++i) {
        move = possible_moves[i];
        if (from == get_move_from(move) &&
                to == get_move_to(move) &&
                get_move_promote(move) == promote_type)
            return move;
    }
    return NO_MOVE;
}
