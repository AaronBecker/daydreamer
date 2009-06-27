
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "grasshopper.h"
    
static const char glyphs[] = " PNBRQK  pnbrqk";

/**
 * Console command handling.
 */
void handle_setboard(position_t* pos, char* command)
{
    while(isspace(*command)) ++command;
    set_position(pos, command);
}

void handle_print(position_t* pos, char* command)
{
    (void)command;
    print_board(pos);
}

void handle_move(position_t* pos, char* command)
{
    while(isspace(*command)) ++command;
    move_t move;
    move = parse_la_move(pos, command);
    if (!move) {
        printf("invalid move\n");
        return;
    }
    undo_info_t undo;
    do_move(pos, move, &undo);
}

void handle_undo(position_t* pos, char* command)
{
    (void)pos;
    (void)command;
    printf("not implemented\n");
}

void handle_moves(position_t* pos, char* command)
{
    (void)command;
    move_t moves[256];
    int num_moves = generate_moves(pos, moves);
    printf("%d pseduo-legal moves:\n", num_moves);
    print_la_move_list(moves);
    num_moves = generate_legal_moves(pos, moves);
    printf("%d legal moves:\n", num_moves);
    print_la_move_list(moves);
}

void handle_perft(position_t* pos, char* command)
{
    int depth;
    while(isspace(*command)) ++command;
    sscanf(command, "%d", &depth);
    perft(pos, depth, false);
}

void handle_divide(position_t* pos, char* command)
{
    int depth;
    while(isspace(*command)) ++command;
    sscanf(command, "%d", &depth);
    perft(pos, depth, true);
}

void handle_quit(position_t* pos, char* command)
{
    (void)pos;
    (void)command;
    exit(0);
}

void handle_showfen(position_t* pos, char* command)
{
    (void)command;
    char fen_str[128];
    position_to_fen_str(pos, fen_str);
    printf("%s\n", fen_str);
}

static void handle_perftsuite(position_t* pos, char* command)
{
    (void)pos;
    while(isspace(*command)) ++command;
    perft_testsuite(command);
}

static char* command_prefixes[] = {
    "perftsuite",
    "setboard",
    "showfen",
    "divide",
    "moves",
    "perft",
    "print",
    "move",
    "undo",
    "quit",
    NULL
};
static int command_prefix_lengths[] = {
    10, 8, 7, 6, 5, 5, 5, 4, 4, 4, 0
};
typedef void(*command_handler)(position_t*, char*);
static command_handler handlers[] = {
    &handle_perftsuite,
    &handle_setboard,
    &handle_showfen,
    &handle_divide,
    &handle_moves,
    &handle_perft,
    &handle_print,
    &handle_move,
    &handle_undo,
    &handle_quit
};

void handle_console(position_t* pos, char* command)
{
    for (int i=0; command_prefixes[i]; ++i) {
        if (!strncasecmp(command,
                    command_prefixes[i],
                    command_prefix_lengths[i])) {
            handlers[i](pos, command+command_prefix_lengths[i]);
            return;
        }
    }
}

/*
 * Text conversion functions.
 */
void move_to_la_str(move_t move, char* str)
{
    square_t from = get_move_from(move);
    square_t to = get_move_to(move);
    str += snprintf(str, 5, "%c%c%c%c",
           (char)square_file(from) + 'a', (char)square_rank(from) + '1',
           (char)square_file(to) + 'a', (char)square_rank(to) + '1');
    if (get_move_promote(move)) {
        snprintf(str, 2, "%c", tolower(glyphs[get_move_promote(move)]));
    }
}

void position_to_fen_str(position_t* pos, char* fen)
{
    int empty_run=0;
    for (square_t square=A8;; ++square) {
        if (empty_run && (pos->board[square] || !valid_board_index(square))) {
            *fen++ = empty_run + '0';
            empty_run = 0;
        }
        if (!valid_board_index(square)) {
            *fen++ = '/';
            square -= 0x19; // drop down to next rank
        } else if (pos->board[square]) {
            *fen++ = glyphs[pos->board[square]->piece];
        } else empty_run++;
        if (square == H1) break;
    }
    *fen++ = ' ';
    *fen++ = pos->side_to_move == WHITE ? 'w' : 'b';
    *fen++ = ' ';
    if (pos->castle_rights == CASTLE_NONE) *fen++ = '-';
    else {
        if (has_oo_rights(pos, WHITE)) *fen++ = 'K';
        if (has_ooo_rights(pos, WHITE)) *fen++ = 'Q';
        if (has_oo_rights(pos, BLACK)) *fen++ = 'k';
        if (has_ooo_rights(pos, BLACK)) *fen++ = 'q';
    }
    *fen++ = ' ';
    if (valid_board_index(pos->ep_square)) {
        *fen++ = square_file(pos->ep_square) + 'a';
        *fen++ = square_rank(pos->ep_square) + '1';
    } else *fen++ = '-';
    *fen++ = ' ';
    fen += sprintf(fen, "%d", pos->fifty_move_counter);
    *fen++ = ' ';
    fen += sprintf(fen, "%d", pos->ply/2);
    *fen = '\0';
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
        printf("\n");
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
