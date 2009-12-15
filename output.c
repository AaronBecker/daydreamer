
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
static void print_pv(search_data_t* data, int ordinal, int index)
{
    const move_t* pv = data->root_moves[index].pv;
    const int depth = data->current_depth;
    const int seldepth = data->stats.max_depth;
    const int score = data->root_moves[index].score;
    // note: use time+1 to avoid divide-by-zero
    const int time = elapsed_time(&data->timer) + 1;
    const uint64_t nodes = data->nodes_searched;

    char sanpv[1024];
    line_to_san_str(&data->root_pos, (move_t*)pv, sanpv);
    printf("info string sanpv %s\n", sanpv);
    if (is_mate_score(score) || is_mated_score(score)) {
        printf("info multipv %d depth %d seldepth %d score mate %d time %d "
                "nodes %"PRIu64" qnodes %"PRIu64" pvnodes %"PRIu64
                " nps %"PRIu64" hashfull %d tbhits %d pv ",
                ordinal, depth, seldepth,
                (MATE_VALUE-abs(score)+1)/2 * (score < 0 ? -1 : 1),
                time, nodes, data->qnodes_searched, data->pvnodes_searched,
                nodes/(time+1)*1000, get_hashfull(), data->stats.egbb_hits);
    } else {
        printf("info multipv %d depth %d seldepth %d score cp %d time %d "
                "nodes %"PRIu64" qnodes %"PRIu64" pvnodes %"PRIu64
                " nps %"PRIu64" hashfull %d tbhits %d pv ",
                ordinal, depth, seldepth, score, time,
                nodes, data->qnodes_searched, data->pvnodes_searched,
                nodes/(time+1)*1000, get_hashfull(), data->stats.egbb_hits);
    }
    int moves = print_coord_move_list(pv);
    if (moves < depth) {
        // If our pv is shortened by a hash hit,
        // try to get more moves from the hash table.
        position_t pos;
        undo_info_t undo;
        copy_position(&pos, &data->root_pos);
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
}

/*
 * Print the n best PVs found during search.
 */
void print_multipv(search_data_t* data)
{
    int indices[256];
    int scores[256];
    int i;
    for (i=0; data->root_moves[i].move; ++i) {
        indices[i] = i;
        scores[i] = data->root_moves[i].score;
    }
    indices[i] = -1;

    for (i=0; indices[i] != -1; ++i) {
        int index = indices[i];
        int score = scores[i];
        int j = i-1;
        while (j >= 0 && scores[j] < score) {
            scores[j+1] = scores[j];
            indices[j+1] = indices[j];
            --j;
        }
        scores[j+1] = score;
        indices[j+1] = index;
    }

    for (i=0; i<options.multi_pv; ++i) print_pv(data, i+1, indices[i]);
}


void print_search_stats(const search_data_t* search_data)
{
    printf("info string cutoffs ");
    for (int i=0; i<=search_data->current_depth; ++i) {
        printf("%d ", search_data->stats.cutoffs[i]);
    }

    printf("\ninfo string move selection ");
    int total_moves = search_data->nodes_searched;
    int hist_moves = 0;
    for (int i=0; i<HIST_BUCKETS; ++i) {
        hist_moves += search_data->stats.move_selection[i];
    }
    printf("%.2f ", (float)(total_moves-hist_moves)/total_moves);
    for (int i=0; i<HIST_BUCKETS; ++i) {
        printf("%.2f ", (float)search_data->stats.move_selection[i] /
                total_moves);
    }

    printf("\ninfo string pv move selection ");
    total_moves = search_data->pvnodes_searched;
    hist_moves = 0;
    for (int i=0; i<HIST_BUCKETS; ++i) {
        hist_moves += search_data->stats.pv_move_selection[i];
    }
    printf("%.2f ", (float)(total_moves-hist_moves)/total_moves);
    for (int i=0; i<HIST_BUCKETS; ++i) {
        printf("%.2f ", (float)search_data->stats.pv_move_selection[i] /
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
    printf("info string scores by iteration ");
    for (int i=0; i<=search_data->current_depth; ++i) {
        printf("%d ", search_data->scores_by_iteration[i]);
    }
    printf("\n");
    if (search_data->obvious_move) {
        printf("info string this move seemed obvious\n");
    }
    int high = search_data->stats.root_fail_highs;
    int low = search_data->stats.root_fail_lows;
    printf("info string root fail high %d root fail low %d root exact %d\n",
            high, low, search_data->current_depth-high-low);
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


