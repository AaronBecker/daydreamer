
#include "daydreamer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
    
extern search_data_t root_data;

/*
 * Console command handling.
 */

static const char* command_prefixes[];
static const int command_prefix_lengths[];
typedef void(*command_handler)(position_t*, char*);
static const command_handler handlers[];

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
    move = coord_str_to_move(pos, command);
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
    print_board(pos, false);
    move_t moves[255];
    printf("moves: ");
    generate_legal_moves(pos, moves);
    for (move_t* move = moves; *move; ++move) {
        char san[8];
        move_to_san_str(pos, *move, san);
        printf("%s ", san);
    }
    printf("\n");
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
            pos->piece_square_eval[WHITE].midgame,
            pos->piece_square_eval[BLACK].midgame);
    report_eval(pos);
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
    print_coord_move_list(moves);
    num_moves = generate_legal_moves(pos, moves);
    printf("%d legal moves:\n", num_moves);
    print_coord_move_list(moves);
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
    move_t move = coord_str_to_move(pos, command);
    printf("see: %d\n", static_exchange_eval(pos, move));
}

/*
 * Command: epd <filename>
 * Run analysis on each position in the given epd file.
 */
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
 * Command: bench <depth>
 * Search all benchmark position to the given depth.
 */
static void handle_bench(position_t* pos, char* command)
{
    (void)pos;
    int depth = 0;
    sscanf(command, " %d", &depth);
    if (!depth) depth = 5;
    benchmark(depth, 0);
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
    "bench",
    "eval",
    "undo",
    "quit",
    "uci",
    "see",
    "epd",
    NULL
};

static const int command_prefix_lengths[] = {
    10, 8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 3, 3, 3, 0
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
    &handle_bench,
    &handle_eval,
    &handle_undo,
    &handle_quit,
    &handle_uci,
    &handle_see,
    &handle_epd
};


