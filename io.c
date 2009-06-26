
#include <ctype.h>
#include <stdio.h>
#include "grasshopper.h"
    
static const char glyphs[] = " PNBRQK  pnbrqk";

void move_to_la_str(move_t move, char* str)
{
    square_t from = get_move_from(move);
    square_t to = get_move_to(move);
    str += snprintf(str, 5, "%c%c%c%c",
           (char)square_file(from) + 'a', (char)square_rank(from) + '1',
           (char)square_file(to) + 'a', (char)square_rank(to) + '1');
    if (get_move_promote(move)) {
        snprintf(str, 2, "%c", tolower(glyphs[get_move_piece(move)]));
    }
}

void print_la_move(move_t move)
{
    static char move_str[6];
    move_to_la_str(move, move_str);
    printf("%s", move_str);
}

void print_la_move_list(const move_t* move)
{
    while(*move) {
        print_la_move(*move++);
        printf(" ");
    }
    printf("\n");
}

void print_board(const position_t* pos)
{
    for (square_t sq = A8; sq != INVALID_SQUARE; ++sq) {
        if (!valid_board_index(sq)) {
            printf("\n");
            if (sq < 0x18) return;
            sq -= 0x19;
            continue;
        }
        printf("%c ", glyphs[pos->board[sq] ? pos->board[sq]->piece : EMPTY]);
    }
}

// unimplemented
void move_to_san_str(position_t* pos, move_t move, char* str);
void position_to_fen_str(position_t* pos, char* str);
