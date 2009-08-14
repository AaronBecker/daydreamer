
#include "daydreamer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
    
extern search_data_t root_data;
static const char glyphs[] = ".PNBRQK  pnbrqk";

/**
 * Console command handling.
 */

static const char* command_prefixes[];
static const int command_prefix_lengths[];
typedef void(*command_handler)(position_t*, char*);
static const command_handler handlers[];
static int bios_key(void);

/*
 * Take a command string and dispatch the appropriate handler function.
 * The command is checked to see if it starts with a recongnized command
 * string. If it does, the appropriate handler function is called with the
 * remainder of the command as an argument.
 */
void handle_console(position_t* pos, char* command)
{
    while(isspace(*command)) ++command;
    for (int i=0; command_prefixes[i]; ++i) {
        if (!strncasecmp(command,
                    command_prefixes[i],
                    command_prefix_lengths[i])) {
            handlers[i](pos, command+command_prefix_lengths[i]);
            return;
        }
    }
    // try to treat the input as a long algebraic move
    move_t move;
    move = parse_la_move(pos, command);
    if (move == NO_MOVE) {
        printf("unrecognized command\n");
        return;
    }
    undo_info_t undo;
    do_move(pos, move, &undo);
}

/*
 * Command: setboard <fen>
 * Sets the board to the given FEN position.
 */
static void handle_setboard(position_t* pos, char* command)
{
    while(isspace(*command)) ++command;
    set_position(pos, command);
}

/*
 * Command: print
 * Prints an ascii represenation of the current position.
 */
static void handle_print(position_t* pos, char* command)
{
    (void)command;
    print_board(pos);
}

/*
 * Command: eval
 * Prints static evaluation of the current position in centipawns.
 */
static void handle_eval(position_t* pos, char* command)
{
    (void)command;
    int eval = full_eval(pos);
    printf("evaluation: %d\n", eval);
    printf("material: (%d,%d)\npiece square: (%d,%d)\n",
            pos->material_eval[WHITE],
            pos->material_eval[BLACK],
            pos->piece_square_eval[WHITE],
            pos->piece_square_eval[BLACK]);
    // make sure black eval is inverse of white
    pos->side_to_move ^=1;
    assert(full_eval(pos) == -eval);
    pos->side_to_move ^=1;
}

static void handle_undo(position_t* pos, char* command)
{
    (void)pos;
    (void)command;
    printf("not implemented\n");
}

/*
 * Command: moves
 * Print all legal and pseudo-legal moves in the current position.
 */
static void handle_moves(position_t* pos, char* command)
{
    (void)command;
    move_t moves[256];
    int num_moves = generate_pseudo_moves(pos, moves);
    printf("%d pseduo-legal moves:\n", num_moves);
    print_la_move_list(moves);
    num_moves = generate_legal_moves(pos, moves);
    printf("%d legal moves:\n", num_moves);
    print_la_move_list(moves);
}

/*
 * Command: perft <depth>
 * Count the nodes at depth <depth> in the current game tree.
 */
static void handle_perft(position_t* pos, char* command)
{
    int depth;
    while(isspace(*command)) ++command;
    sscanf(command, "%d", &depth);
    perft(pos, depth, false);
}

/*
 * Command: divide <depth>
 * Count the nodes at depth <depth> in the current game tree, separating out
 * the results for each move.
 */
static void handle_divide(position_t* pos, char* command)
{
    int depth;
    while(isspace(*command)) ++command;
    sscanf(command, "%d", &depth);
    perft(pos, depth, true);
}

/*
 * Command: quit
 * Exit the program.
 */
static void handle_quit(position_t* pos, char* command)
{
    (void)pos;
    (void)command;
    exit(0);
}

/*
 * Command: showfen
 * Print the FEN representation of the current position.
 */
static void handle_showfen(position_t* pos, char* command)
{
    (void)command;
    char fen_str[128];
    position_to_fen_str(pos, fen_str);
    printf("%s\n", fen_str);
}

/*
 * Command: perftsuite <filename>
 * Run the perftsuite in the file with the given name.
 */
static void handle_perftsuite(position_t* pos, char* command)
{
    (void)pos;
    while(isspace(*command)) ++command;
    perft_testsuite(command);
}

/*
 * Command: search <depth>
 * Search the current position to the given depth.
 */
static void handle_search(position_t* pos, char* command)
{
    while(isspace(*command)) ++command;
    int depth;
    sscanf(command, "%d", &depth);
    extern search_data_t root_data;
    copy_position(&root_data.root_pos, pos);
    init_search_data(&root_data);
    root_data.depth_limit = depth;
    root_data.infinite = true;
    deepening_search(&root_data);
}

/*
 * Command: see <move>
 * Print the static exchange evaluation for the given capturing move.
 */
static void handle_see(position_t* pos, char* command)
{
    while (isspace(*command)) ++command;
    move_t capture = parse_la_move(pos, command);
    printf("see: %d\n", static_exchange_eval(pos, capture));
}

static void handle_epd(position_t* pos, char* command)
{
    (void)pos;
    char filename[256];
    int time_per_move = 5;
    sscanf(command, "%s %d", filename, &time_per_move);
    time_per_move *= 1000;
    epd_testsuite(filename, time_per_move);
}


/*
 * Command: uci
 * Switch to uci protocol.
 */
static void handle_uci(position_t* pos, char* command)
{
    (void)pos;
    (void)command;
    uci_main();
}

static const char* command_prefixes[] = {
    "perftsuite",
    "setboard",
    "showfen",
    "divide",
    "search",
    "moves",
    "perft",
    "print",
    "eval",
    "undo",
    "quit",
    "uci",
    "see",
    "epd",
    NULL
};

static const int command_prefix_lengths[] = {
    10, 8, 7, 6, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 0
};

static const command_handler handlers[] = {
    &handle_perftsuite,
    &handle_setboard,
    &handle_showfen,
    &handle_divide,
    &handle_search,
    &handle_moves,
    &handle_perft,
    &handle_print,
    &handle_eval,
    &handle_undo,
    &handle_quit,
    &handle_uci,
    &handle_see,
    &handle_epd
};

/**
 * Text conversion functions.
 */

/*
 * Convert a square into ascii coordinates.
 */
int square_to_coord_str(square_t sq, char* str)
{
    *str++ = (char)square_file(sq) + 'a';
    *str++ = (char)square_rank(sq) + '1';
    *str = '\0';
    return 2;
}

/*
 * Convert a move to its long algebraic string form.
 */
void move_to_la_str(move_t move, char* str)
{
    if (move == NO_MOVE) {
        strcpy(str, "(none)");
        return;
    }
    if (move == NULL_MOVE) {
        strcpy(str, "(null)");
        return;
    }
    square_t from = get_move_from(move);
    square_t to = get_move_to(move);
    str += snprintf(str, 5, "%c%c%c%c",
           (char)square_file(from) + 'a', (char)square_rank(from) + '1',
           (char)square_file(to) + 'a', (char)square_rank(to) + '1');
    if (get_move_promote(move)) {
        snprintf(str, 2, "%c", tolower(glyphs[get_move_promote(move)]));
    }
}

/*
 * Convert a position to its FEN form.
 * (see wikipedia.org/wiki/Forsyth-Edwards_Notation)
 */
void position_to_fen_str(const position_t* pos, char* fen)
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
    if (pos->ep_square != EMPTY && valid_board_index(pos->ep_square)) {
        *fen++ = square_file(pos->ep_square) + 'a';
        *fen++ = square_rank(pos->ep_square) + '1';
    } else *fen++ = '-';
    *fen++ = ' ';
    fen += sprintf(fen, "%d", pos->fifty_move_counter);
    *fen++ = ' ';
    fen += sprintf(fen, "%d", pos->ply/2);
    *fen = '\0';
}

/*
 * Print the long algebraic form of |move| to stdout.
 */
void print_la_move(move_t move)
{
    static char move_str[6];
    move_to_la_str(move, move_str);
    printf("%s ", move_str);
}

/*
 * Print a null-terminated list of moves to stdout, returning the number of
 * nodes printed.
 */
int print_la_move_list(const move_t* move)
{
    int moves=0;
    while(*move) {
        print_la_move(*move++);
        ++moves;
    }
    return moves;
}

/*
 * Print a principal variation in uci format.
 */
void print_pv(search_data_t* search_data)
{
    const move_t* pv = search_data->pv;
    const int depth = search_data->current_depth;
    const int score = search_data->best_score;
    const int time = elapsed_time(&search_data->timer);
    const uint64_t nodes = search_data->nodes_searched;

    // note: use time+1 avoid divide-by-zero
    // mate scores are given as MATE_VALUE-ply, so we can calculate depth
    if (is_mate_score(score)) {
        printf("info depth %d score mate %d time %d nodes %"PRIu64\
                " qnodes %"PRIu64" nps %"PRIu64" pv ",
                depth,
                (MATE_VALUE-abs(score)+1)/2 * (score < 0 ? -1 : 1),
                time,
                nodes,
                search_data->qnodes_searched,
                nodes/(time+1)*1000);
    } else {
        printf("info depth %d score cp %d time %d nodes %"PRIu64\
                " qnodes %"PRIu64" nps %"PRIu64" pv ",
                depth, score, time, nodes, search_data->qnodes_searched, nodes/(time+1)*1000);
    }
    int moves = print_la_move_list(pv);
    if (moves < depth) {
        // If our pv is shortened by a hash hit,
        // try to get more moves from the hash table.
        position_t pos;
        undo_info_t undo;
        copy_position(&pos, &search_data->root_pos);
        for (int i=0; pv[i] != NO_MOVE; ++i) do_move(&pos, pv[i], &undo);

        transposition_entry_t* entry;
        while (moves < depth) {
            entry = get_transposition_entry(&pos);
            if (!entry || !is_move_legal(&pos, entry->move)) break;
            print_la_move(entry->move);
            do_move(&pos, entry->move, &undo);
            ++moves;
        }
    }
    printf("\n");
    char sanpv[1024];
    line_to_san_str(&search_data->root_pos, (move_t*)pv, sanpv);
    printf("info string sanpv %s\n", sanpv);
}

/*
 * Print an ascii representation of the current board.
 */
void print_board(const position_t* pos)
{
    char fen_str[256];
    position_to_fen_str(pos, fen_str);
    printf("fen: %s\n", fen_str);
    printf("hash: %"PRIx64"\n", pos->hash);
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

/*
 * Look for and handle console commands (just uci at the moment). Called
 * periodically during search.
 */
void check_for_input(search_data_t* search_data)
{
    char input[4096];
    int data = bios_key();
    if (data) {
        if (!fgets(input, 4096, stdin))
            strcpy(input, "quit\n");
        if (!strncasecmp(input, "quit", 4)) {
            search_data->engine_status = ENGINE_ABORTED;
        } else if (!strncasecmp(input, "stop", 4)) {
            search_data->engine_status = ENGINE_ABORTED;
        } else if (strncasecmp(input, "ponderhit", 9) == 0) {
            // TODO: handle ponderhits
        }
    }
}

/*
 * Boilerplate code to see if data is available to be read on stdin.
 * Cross-platform for unix/windows.
 *
 * Many thanks to the original author(s). I've seen minor variations on this
 * in Scorpio, Viper, Beowulf, Olithink, and others, so I don't know where
 * it's from originally.
 */
#ifndef _WIN32
/* Unix version */
int bios_key(void)
{
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(fileno(stdin), &readfds);
    /* Set to timeout immediately */
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    select(16, &readfds, 0, 0, &timeout);

    return (FD_ISSET(fileno(stdin), &readfds));
}

#else
/* Windows version */
#include <conio.h>
int bios_key(void)
{
    static int init=0, pipe=0;
    static HANDLE inh;
    DWORD dw;
    /*
     * If we're running under XBoard then we can't use _kbhit() as the input
     * commands are sent to us directly over the internal pipe
     */
#if defined(FILE_CNT)
    if (stdin->_cnt > 0) return stdin->_cnt;
#endif
    if (!init) {
        init = 1;
        inh = GetStdHandle(STD_INPUT_HANDLE);
        pipe = !GetConsoleMode(inh, &dw);
        if (!pipe) {
            SetConsoleMode(inh,
                    dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
            FlushConsoleInputBuffer(inh);
        }
    }
    if (pipe) {
        if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) {
            return 1;
        }
        return dw;
    } else {
        GetNumberOfConsoleInputEvents(inh, &dw);
        return dw <= 1 ? 0 : dw;
    }
}
#endif


