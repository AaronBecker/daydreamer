
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
static const int passed_bonus[2][8] = {
    { 0,  5, 10, 20, 60, 120, 200, 0},
    { 0, 10, 20, 25, 50,  90, 125, 0},
};
static const int candidate_bonus[2][8] = {
    { 0,  5,  5, 10, 20, 30, 0, 0},
    { 0,  5, 10, 20, 45, 70, 0, 0},
};
static const int backward_penalty[2][8] = {
    { 5, 10, 10, 15, 15, 10, 10,  5},
    {20, 20, 20, 20, 20, 20, 20, 20}
};
static const int connected_bonus[2] = {10, 20};
static const int cumulative_defect_penalty[8] = {
//    0, 0, 5, 10, 25, 50, 60, 75
    0, 0, 0, 0, 0, 0, 0, 0
};
static const int unstoppable_passer_bonus[8] = {
    0, 500, 525, 550, 575, 600, 650, 0
};
static const int advanceable_passer_bonus[8] = {
    0, 20, 25, 30, 35, 40, 80, 0
};

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
    pd->score[WHITE].midgame = pd->score[WHITE].endgame = 0;
    pd->score[BLACK].midgame = pd->score[BLACK].endgame = 0;
    square_t sq;
    for (color_t color=WHITE; color<=BLACK; ++color) {
        pd->num_passed[color] = 0;
        int push = pawn_push[color];
        const piece_t pawn = create_piece(color, PAWN);
        const piece_t opp_pawn = create_piece(color^1, PAWN);
        int num_defects = 0;
        for (int i=0; pos->pawns[color][i] != INVALID_SQUARE; ++i) {
            sq = pos->pawns[color][i];
            file_t file = square_file(sq);
            rank_t rank = relative_rank[color][square_rank(sq)];
            
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
                pd->score[color].midgame += passed_bonus[0][rank];
                pd->score[color].endgame += passed_bonus[1][rank];
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
                    pd->score[color].midgame += candidate_bonus[0][rank];
                    pd->score[color].endgame += candidate_bonus[1][rank];
                }
            }

            // Doubled pawns.
            for (to = sq+push; pos->board[to] != OUT_OF_BOUNDS; to += push) {
                if (pos->board[to] == pawn) {
                    pd->score[color].midgame -= doubled_penalty[0][file];
                    pd->score[color].endgame -= doubled_penalty[1][file];
                    ++num_defects;
                    break;
                }
            }

            // Isolated pawns.
            bool isolated = false;
            to = file + N;
            for (; pos->board[to] != OUT_OF_BOUNDS; to += N) {
                if (pos->board[to-1] == pawn || pos->board[to+1] == pawn) {
                    pd->score[color].midgame -= isolation_penalty[0][file];
                    pd->score[color].endgame -= isolation_penalty[1][file];
                    isolated = true;
                    ++num_defects;
                    break;
                }
            }

            // Connected pawns.
            if (((pos->board[sq+1] == pawn && pos->board[sq-1] == pawn) ||
                    pos->board[sq+push+1] == pawn ||
                    pos->board[sq+push-1] == pawn)) {
                pd->score[color].midgame += connected_bonus[0];
                pd->score[color].endgame += connected_bonus[1];
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
                        pd->score[color].midgame -= backward_penalty[0][file];
                        pd->score[color].endgame -= backward_penalty[1][file];
                        ++num_defects;
                    }
                }
            }
        }
        int defect_penalty = cumulative_defect_penalty[MIN(7, num_defects)];
        pd->score[color].midgame -= defect_penalty;
        pd->score[color].endgame -= defect_penalty;
    }
    return pd;
}

score_t pawn_score(const position_t* pos)
{
    pawn_data_t* pd = analyze_pawns(pos);
    int passer_bonus[2] = {0, 0};
    for (color_t side=WHITE; side<=BLACK; ++side) {
        for (int i=0; i<pd->num_passed[side]; ++i) {
            square_t passer = pd->passed[side][i];
            rank_t rank = relative_rank[side][square_rank(passer)];
            if (pos->num_pieces[side^1] == 1) {
                // Other side is down to king+pawns. Is this passer stoppable?
                // This measure is conservative, which is fine.
                int prom_dist = 8 - rank;
                if (rank == RANK_2) --prom_dist;
                if (pos->side_to_move == side) --prom_dist;
                square_t prom_sq = create_square(square_file(passer), RANK_8);
                if (distance(pos->pieces[side^1][0], prom_sq) > prom_dist) {
                    passer_bonus[side] += unstoppable_passer_bonus[rank];
                }
            } else {
                // Can the pawn advance without being captured?
                square_t target = passer + pawn_push[side];
                if (pos->board[target] != EMPTY) continue;
                move_t push = rank == RANK_7 ?
                    create_move_promote(passer, target,
                            create_piece(side, PAWN), EMPTY, QUEEN) :
                    create_move(passer, target,
                            create_piece(side, PAWN), EMPTY);
                if (static_exchange_eval(pos, push) < 0) continue;
                passer_bonus[side] += advanceable_passer_bonus[rank];
            }
        }
    }
    color_t side = pos->side_to_move;
    score_t score;
    score.midgame = pd->score[side].midgame + passer_bonus[side] -
        (pd->score[side^1].midgame + passer_bonus[side^1]);
    score.endgame = pd->score[side].endgame + passer_bonus[side] -
        (pd->score[side^1].endgame + passer_bonus[side^1]);
    return score;
}

