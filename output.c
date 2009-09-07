
#include "daydreamer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
    
extern const char glyphs[];

/*
 * Print the coordinate form of |move| to stdout.
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
 * Print a coordinate square to stdout.
 */
void print_coord_square(square_t square)
{
    static char square_str[2];
    square_to_coord_str(square, square_str);
    printf("%s ", square_str);
}

/*
 * Print a principal variation in uci format.
 */
void print_pv(search_data_t* search_data)
{
    const move_t* pv = search_data->pv;
    const int depth = search_data->current_depth;
    const int score = search_data->best_score;
    // note: use time+1 avoid divide-by-zero
    const int time = elapsed_time(&search_data->timer) + 1;
    const uint64_t nodes = search_data->nodes_searched;

    char sanpv[1024];
    line_to_san_str(&search_data->root_pos, (move_t*)pv, sanpv);
    printf("info string sanpv %s\n", sanpv);
    if (is_mate_score(score) || is_mated_score(score)) {
        printf("info depth %d score mate %d time %d nodes %"PRIu64\
                " qnodes %"PRIu64" nps %"PRIu64" tbhits %d pv ",
                depth,
                (MATE_VALUE-abs(score)+1)/2 * (score < 0 ? -1 : 1),
                time,
                nodes,
                search_data->qnodes_searched,
                nodes/(time+1)*1000,
                search_data->stats.egbb_hits);
    } else {
        printf("info depth %d score cp %d time %d nodes %"PRIu64\
                " qnodes %"PRIu64" nps %"PRIu64" tbhits %d pv ",
                depth, score, time, nodes,
                search_data->qnodes_searched, nodes/(time+1)*1000,
                search_data->stats.egbb_hits);
    }
    int moves = print_coord_move_list(pv);
    // FIXME: re-enable this
    //if (moves < depth) {
    //    // If our pv is shortened by a hash hit,
    //    // try to get more moves from the hash table.
    //    position_t pos;
    //    undo_info_t undo;
    //    copy_position(&pos, &search_data->root_pos);
    //    for (int i=0; pv[i] != NO_MOVE; ++i) do_move(&pos, pv[i], &undo);

    //    transposition_entry_t* entry;
    //    while (moves < depth) {
    //        entry = get_transposition(&pos);
    //        if (!entry || !is_move_legal(&pos, entry->move)) break;
    //        print_coord_move(entry->move);
    //        do_move(&pos, entry->move, &undo);
    //        ++moves;
    //    }
    //}
    printf("\n");
}

void print_search_stats(const search_data_t* search_data)
{
    printf("info string cutoffs ");
    for (int i=0; i<=search_data->current_depth; ++i) {
        printf("%d ", search_data->stats.cutoffs[i]);
    }
    printf("\ninfo string move selection ");
    int total_moves = 0;
    for (int i=0; i<HIST_BUCKETS; ++i) {
        total_moves += search_data->stats.move_selection[i];
    }
    for (int i=0; i<HIST_BUCKETS; ++i) {
        printf("%.2f ", (float)search_data->stats.move_selection[i] /
                total_moves);
    }
    printf("\ninfo string razoring attempts/cutoffs by depth "
            "%d/%d %d/%d %d/%d\n",
        search_data->stats.razor_attempts[0],
        search_data->stats.razor_prunes[0],
        search_data->stats.razor_attempts[1],
        search_data->stats.razor_prunes[1],
        search_data->stats.razor_attempts[2],
        search_data->stats.razor_prunes[2]);
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
            if (sq < 0x18) break;
            if (uci_prefix) printf("info string ");
            sq -= 0x19;
            continue;
        }
        printf("%c ", glyphs[pos->board[sq]]);
    }
    report_eval(pos);
}


