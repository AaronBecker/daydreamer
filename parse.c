
#include <ctype.h>
#include "daydreamer.h"

/*
 * Convert an algebraic string representation of a square (e.g. A1, c6) to
 * a square_t.
 */
square_t parse_coord_square(const char* alg_square)
{
    if (tolower(alg_square[0]) < 'a' || tolower(alg_square[0]) > 'h' ||
        alg_square[1] < '0' || alg_square[1] > '9') return EMPTY;
    return create_square(tolower(alg_square[0])-'a', alg_square[1]-'1');
}

/*
 * Convert a long algebraic move string (e.g. E2E4, c7c8q) to a move_t.
 * Only legal moves are generated--if the given move is impossible, NO_MOVE
 * is returned instead.
 */
move_t parse_coord_move(position_t* pos, const char* coord_move)
{
    square_t from = parse_coord_square(coord_move);
    square_t to = parse_coord_square(coord_move + 2);
    if (from == INVALID_SQUARE || to == INVALID_SQUARE) return NO_MOVE;
    piece_type_t promote_type = NONE;
    switch (*(coord_move+4)) {
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
