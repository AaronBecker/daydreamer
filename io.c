
#include "daydreamer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
    
/**
 * Text conversion functions.
 */

static const char glyphs[] = ".PNBRQK  pnbrqk";

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
 * Convert a move to its coordinate string form.
 */
void move_to_coord_str(move_t move, char* str)
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
void print_coord_move(move_t move)
{
    static char move_str[6];
    move_to_coord_str(move, move_str);
    printf("%s ", move_str);
}

/*
 * Print a null-terminated list of moves to stdout, returning the number of
 * nodes printed.
 */
int print_coord_move_list(const move_t* move)
{
    int moves=0;
    while(*move) {
        print_coord_move(*move++);
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
    int moves = print_coord_move_list(pv);
    if (moves < depth) {
        // If our pv is shortened by a hash hit,
        // try to get more moves from the hash table.
        position_t pos;
        undo_info_t undo;
        copy_position(&pos, &search_data->root_pos);
        for (int i=0; pv[i] != NO_MOVE; ++i) do_move(&pos, pv[i], &undo);

        transposition_entry_t* entry;
        while (moves < depth) {
            entry = get_transposition(&pos);
            if (!entry || !is_move_legal(&pos, entry->move)) break;
            print_coord_move(entry->move);
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
void print_board(const position_t* pos, bool uci_prefix)
{
    char fen_str[256];
    position_to_fen_str(pos, fen_str);
    if (uci_prefix) printf("info string ");
    printf("fen: %s\n", fen_str);
    if (uci_prefix) printf("info string ");
    printf("hash: %"PRIx64"\n", pos->hash);
    if (uci_prefix) printf("info string ");
    for (square_t sq = A8; sq != INVALID_SQUARE; ++sq) {
        if (!valid_board_index(sq)) {
            printf("\n");
            if (sq < 0x18) return;
            if (uci_prefix) printf("info string ");
            sq -= 0x19;
            continue;
        }
        printf("%c ", glyphs[pos->board[sq] ? pos->board[sq]->piece : EMPTY]);
    }
}


