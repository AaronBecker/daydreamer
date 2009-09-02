
#include "daydreamer.h"
#include <string.h>

static const int isolation_penalty[2][8] = {
    {10, 10, 10, 15, 15, 10, 10, 10},
    {20, 20, 20, 20, 20, 20, 20, 20}
};
static const int doubled_penalty[2][8] = {
    { 5, 10, 15, 20, 20, 15, 10,  5},
    {20, 20, 20, 20, 20, 20, 20, 20}
};
static const int backward_penalty[2][8] = {
    { 5, 10, 10, 15, 15, 10, 10,  5},
    {20, 20, 20, 20, 20, 20, 20, 20}
};
static const int connected_bonus[2] = {10, 20};
static const int passed_bonus[2][8] = {
    { 0,  5, 10, 20, 60, 120, 200, 0},
    { 0, 10, 20, 25, 50,  90, 125, 0},
};
static const int candidate_bonus[2][8] = {
    { 0,  5,  5, 20, 35, 50, 0, 0},
    { 0,  5,  5, 20, 45, 70, 0, 0},
};
static const int cumulative_defect_penalty[8] = {0, 0, 5, 10, 25, 50, 60, 75};
// TODO: backward/weak pawns
// TODO: figure out how obstructed passed pawns are in eval

static pawn_data_t* pawn_table = NULL;
static int num_buckets;
static struct {
    int misses;
    int hits;
    int occupied;
    int evictions;
} pawn_hash_stats;

/*
 * Create a pawn hash table of the appropriate size.
 */
void init_pawn_table(const int max_bytes)
{
    assert(max_bytes >= 1024);
    int size = sizeof(pawn_data_t);
    num_buckets = 1;
    while (size <= max_bytes >> 1) {
        size <<= 1;
        num_buckets <<= 1;
    }
    if (pawn_table != NULL) free(pawn_table);
    pawn_table = malloc(size);
    assert(pawn_table);
    clear_pawn_table();
}

/*
 * Wipe the entire table.
 */
void clear_pawn_table(void)
{
    memset(pawn_table, 0, sizeof(pawn_data_t) * num_buckets);
    memset(&pawn_hash_stats, 0, sizeof(pawn_hash_stats));
}

static pawn_data_t* get_pawn_data(const position_t* pos)
{
    pawn_data_t* pd = &pawn_table[pos->pawn_hash % num_buckets];
    if (pd->key == pos->pawn_hash) pawn_hash_stats.hits++;
    else if (pd->key != 0) pawn_hash_stats.evictions++;
    else {
        pawn_hash_stats.misses++;
        pawn_hash_stats.occupied++;
    }
    return pd;
}

void print_pawn_stats(void)
{
    int hits = pawn_hash_stats.hits;
    int probes = hits + pawn_hash_stats.misses + pawn_hash_stats.evictions;
    printf("info string pawn hash entries %d hashfull %d hits %d misses %d "
            "evictions %d hitrate %2.2f\n",
            num_buckets, pawn_hash_stats.occupied*1000/num_buckets,
            hits, pawn_hash_stats.misses,
            pawn_hash_stats.evictions,
            ((float)pawn_hash_stats.hits)/probes);
}

/*
 * Identify and record the position of all passed pawns. Analyze pawn structure
 * features such as isolated and doubled pawns and assign a pawn structure
 * score (which does not account for passers). This information is stored in
 * the pawn hash table, to prevent re-computation.
 */
pawn_data_t* analyze_pawns(const position_t* pos)
{
    pawn_data_t* pd = get_pawn_data(pos);
    if (pd->key == pos->pawn_hash) return pd;

    pd->key = pos->pawn_hash;
    pd->score[0] = pd->score[1] = 0;
    pd->endgame_score[0] = pd->endgame_score[1] = 0;

    square_t sq;
    for (color_t color=WHITE; color<=BLACK; ++color) {
        pd->num_passed[color] = 0;
        int push = pawn_push[color];
        piece_t pawn = create_piece(color, PAWN);
        piece_t opp_pawn = create_piece(color^1, PAWN);
        int num_defects = 0;
        for (int i=0; pos->pawns[color][i] != INVALID_SQUARE; ++i) {
            sq = pos->pawns[color][i];
            file_t file = square_file(sq);
            rank_t rank = relative_pawn_rank[color][square_rank(sq)];
            
            // Passed pawns.
            bool passed = true;
            square_t to = sq + push;
            for (; pos->board[to] != OUT_OF_BOUNDS; to += push) {
                if (pos->board[to-1] == opp_pawn ||
                        pos->board[to] == opp_pawn ||
                        pos->board[to+1] == opp_pawn) {
                    passed = false;
                    break;
                }
            }
            if (passed) {
                pd->passed[color][pd->num_passed[color]++] = sq;
                pd->score[color] += passed_bonus[0][rank];
                pd->endgame_score[color] += passed_bonus[1][rank];
            } else {
                // Candidate passed pawns (one enemy pawn one file away).
                int blockers = 0;
                for (to = sq + push;
                        pos->board[to] != OUT_OF_BOUNDS; to += push) {
                    if (pos->board[to-1] == opp_pawn) ++blockers;
                    if (pos->board[to] == opp_pawn) blockers = 2;
                    if (pos->board[to+1] == opp_pawn) ++blockers;
                }
                if (blockers < 2) {
                    pd->score[color] += candidate_bonus[0][rank];
                    pd->endgame_score[color] += candidate_bonus[1][rank];
                }
            }

            // Doubled pawns.
            for (to = sq+push; pos->board[to] != OUT_OF_BOUNDS; to += push) {
                if (pos->board[to] == pawn) {
                    pd->score[color] -= doubled_penalty[0][file];
                    pd->endgame_score[color] -= doubled_penalty[1][file];
                    ++num_defects;
                    break;
                }
            }

            // Isolated pawns.
            bool isolated = false;
            to = file + N;
            for (; pos->board[to] != OUT_OF_BOUNDS; to += N) {
                if (pos->board[to-1] == pawn || pos->board[to+1] == pawn) {
                    pd->score[color] -= isolation_penalty[0][file];
                    pd->endgame_score[color] -= isolation_penalty[1][file];
                    isolated = true;
                    ++num_defects;
                    break;
                }
            }

            // Connected pawns.
            if (!isolated && (pos->board[sq+1] == pawn ||
                    pos->board[sq+push+1] == pawn ||
                    pos->board[sq+push-1] == pawn)) {
                pd->score[color] += connected_bonus[0];
                pd->endgame_score[color] += connected_bonus[1];
            }

            // Backward pawns (unsupportable by pawns, can't advance)
            if (!passed && !isolated &&
                    pos->board[sq+push-1] != opp_pawn &&
                    pos->board[sq+push+1] != opp_pawn) {
                bool backward = true;
                for (to = sq; pos->board[to] != OUT_OF_BOUNDS; to -= push) {
                    if (pos->board[to-1] == pawn || pos->board[to+1] == pawn) {
                        backward = false;
                        break;
                    }
                }
                if (backward) {
                    for (to = sq + 2*push; pos->board[to] != OUT_OF_BOUNDS;
                            to += push) {
                        if (pos->board[to-1] == opp_pawn ||
                                pos->board[to+1] == opp_pawn) break;
                        if (pos->board[to-1] == pawn ||
                                pos->board[to+1] == pawn) {
                            backward = false;
                            break;
                        }
                    }
                    if (backward) {
                        pd->score[color] -= backward_penalty[0][file];
                        pd->endgame_score[color] -= backward_penalty[1][file];
                        ++num_defects;
                    }
                }
            }
        }
        int defect_penalty = cumulative_defect_penalty[MIN(8, num_defects)];
        pd->score[color] -= defect_penalty;
        pd->endgame_score[color] -= defect_penalty;
    }
    return pd;
}

void pawn_score(const position_t* pos, score_t* score)
{
    pawn_data_t* pd = analyze_pawns(pos);
    color_t side = pos->side_to_move;
    assert(abs(pd->score[WHITE]<2000) && abs(pd->score[BLACK]<2000));
    score->midgame += pd->score[side] - pd->score[side^1];
    score->endgame += pd->endgame_score[side] - pd->endgame_score[side^1];
}

