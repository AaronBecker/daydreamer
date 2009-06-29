
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grasshopper.h"

static void uci_get_input(void);
static void uci_handle_command(char* command);
static void uci_position(char* uci_pos);
static void uci_go(char* command);
static void calculate_search_time(int wtime,
        int btime,
        int winc,
        int binc,
        int movestogo);

void uci_main(void)
{
    printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
    printf("id author %s\n", ENGINE_AUTHOR);
    // TODO: should probably support some options (at least the mandatory ones)
    printf("uciok\n");
    init_search_data();
    set_position(&root_data.root_pos, FEN_STARTPOS);
    while (1) uci_get_input();
}

static void uci_get_input(void)
{
    static char command[1024];
    if (!fgets(command, 1024, stdin)) strcpy(command, "quit\n");
    uci_handle_command(command);
}

static void uci_handle_command(char* command)
{
    if (!strncasecmp(command, "isready", 7)) printf("readyok\n");
    else if (!strncasecmp(command, "quit", 4)) exit(0);
    else if (!strncasecmp(command, "position", 8)) uci_position(command+9);
    else if (!strncasecmp(command, "go", 2)) uci_go(command+3);
    // not handled: setoption name <option>, ucinewgame
}

static void uci_position(char* uci_pos)
{
    while (isspace(*uci_pos)) ++uci_pos;
    if (!strncasecmp(uci_pos, "startpos", 8)) {
        set_position(&root_data.root_pos, FEN_STARTPOS);
        uci_pos += 8;
    } else if (!strncasecmp(uci_pos, "fen", 3)) {
        uci_pos += 3;
        while (*uci_pos && isspace(*uci_pos)) ++uci_pos;
        set_position(&root_data.root_pos, uci_pos);
    }
    while (*uci_pos && isspace(*uci_pos)) ++uci_pos;
    if (!strncasecmp(uci_pos, "moves", 5)) {
        uci_pos += 5;
        while (*uci_pos) {
            while (*uci_pos && isspace(*uci_pos)) ++uci_pos;
            move_t move = parse_la_move(&root_data.root_pos, uci_pos);
            if (move == NO_MOVE) {
                printf("Warning: could not parse %s\n", uci_pos);
                return;
            }
            undo_info_t dummy_undo;
            do_move(&root_data.root_pos, move, &dummy_undo);
            while (*uci_pos && !isspace(*uci_pos)) ++uci_pos;
        }
    }
}

static void uci_go(char* command)
{
    char* info;
    int wtime=0, btime=0, winc=0, binc=0, movestogo=0, movetime=0;

    root_data.infinite = root_data.ponder = false;
    root_data.depth_limit = root_data.node_limit = root_data.mate_search = 0;
    if ((info = strcasestr(command, "searchmoves"))) {
    }
    if ((info = strcasestr(command, "ponder"))) {
        root_data.ponder = true;
    }
    if ((info = strcasestr(command, "wtime"))) {
        sscanf(info+5, " %d", &wtime);
    }
    if ((info = strcasestr(command, "btime"))) {
        sscanf(info+5, " %d", &btime);
    }
    if ((info = strcasestr(command, "winc"))) {
        sscanf(info+4, " %d", &winc);
    }
    if ((info = strcasestr(command, "binc"))) {
        sscanf(info+4, " %d", &binc);
    }
    if ((info = strcasestr(command, "movestogo"))) {
        sscanf(info+9, " %d", &movestogo);
    }
    if ((info = strcasestr(command, "depth"))) {
        sscanf(info+5, " %d", &root_data.depth_limit);
    }
    if ((info = strcasestr(command, "nodes"))) {
        sscanf(info+5, " %llu", &root_data.node_limit);
    }
    if ((info = strcasestr(command, "mate"))) {
        sscanf(info+4, " %d", &root_data.mate_search);
    }
    if ((info = strcasestr(command, "movetime"))) {
        sscanf(info+8, " %d", &movetime);
        root_data.time_target = root_data.time_limit = movetime;
    }
    if ((info = strcasestr(command, "infinite"))) {
        root_data.infinite = true;
    }

    if (!movetime && !root_data.infinite) {
        calculate_search_time(wtime, btime, winc, binc, movestogo);
    }
    root_search();
}

static void calculate_search_time(int wtime,
        int btime,
        int winc,
        int binc,
        int movestogo)
{
    // TODO: cool heuristics for time mangement.
    // For now, just use a simple static rule and look at our own time only
    color_t side = root_data.root_pos.side_to_move;
    int inc = side == WHITE ? winc : binc;
    int time = side == WHITE ? wtime : btime;
    // Try to use 1/n of our remaining time, where there are n moves left.
    // If we don't know n, guess 40. Allow using up to 8/n for tough moves.
    if (!movestogo) movestogo = 40;
    time += movestogo*inc;
    root_data.time_target = time/movestogo;
    root_data.time_limit = time < time*8/movestogo ? time : time*8/movestogo;
}
