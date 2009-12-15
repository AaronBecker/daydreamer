
#include "daydreamer.h"
#include <string.h>

// TODO: Tune these values, in particular the endgame values.
// TODO: bonus/penalty for occupying a lot of space.
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
static const int unstoppable_passer_bonus[8] = {
    0, 500, 525, 550, 575, 600, 650, 0
};
static const int advanceable_passer_bonus[8] = {
    0, 20, 25, 30, 35, 40, 80, 0
};
static const int king_storm[0x80] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,-10,-10,-10,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0, -8, -8, -8,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  4,  4,  4,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  8,  8,  8,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0, 12, 12, 12,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0, 14, 16, 14,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};
static const int queen_storm[0x80] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  -10,-10,-10, -5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   -8, -8, -8, -4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    4,  4,  4,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    8,  8,  8,  4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   12, 12, 12,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   14, 16, 14,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};
static const int king_storm_open_file[8] = {
    0, 0, 0, 0, 0, 10, 15, 15
};
static const int queen_storm_open_file[8] = {
    10, 15, 10, 0, 0, 0, 0, 0
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

/*
 * Look up the pawn data for the pawns in the given position.
 */
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

/*
 * Print stats about the pawn hash.
 */
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

    // Zero everything out and create pawn bitboards.
    memset(pd, 0, sizeof(pawn_data_t));
    pd->key = pos->pawn_hash;
    square_t sq, to;
    for (color_t color=WHITE; color<=BLACK; ++color) {
        for (int i=0; pos->pawns[color][i] != INVALID_SQUARE; ++i) {
            set_sq_bit(pd->pawns_bb[color], pos->pawns[color][i]);
        }
    }

    // Create outpost bitboard and analyze pawns.
    for (color_t color=WHITE; color<=BLACK; ++color) {
        pd->num_passed[color] = 0;
        int push = pawn_push[color];
        const piece_t pawn = create_piece(color, PAWN);
        const piece_t opp_pawn = create_piece(color^1, PAWN);
        bitboard_t our_pawns = pd->pawns_bb[color];
        bitboard_t their_pawns = pd->pawns_bb[color^1];

        // Give pawn storm bonuses for open files
        for (int file=0; file<7; ++file) {
            if (!(our_pawns & file_mask[file])) {
                pd->kingside_storm[color] += king_storm_open_file[file];
                pd->queenside_storm[color] += queen_storm_open_file[file];
            }
        }
        
        for (int ind=0; ind<64; ++ind) {
            // Fill in mask of outpost squares.
            sq = index_to_square(ind);
            if (!(outpost_mask[color][ind] & their_pawns)) {
                set_bit(pd->outposts_bb[color], ind);
            }
            if (pos->board[sq] != create_piece(color, PAWN)) continue;

            // Pawn analysis.
            file_t file = square_file(sq);
            rank_t rank = square_rank(sq);
            rank_t rrank = relative_rank[color][rank];

            // Passed pawns and passed pawn candidates.
            bool passed = !(passed_mask[color][ind] & their_pawns);
            if (passed) {
                set_bit(pd->passed_bb[color], ind);
                pd->passed[color][pd->num_passed[color]++] = sq;
                pd->score[color].midgame += passed_bonus[0][rrank];
                pd->score[color].endgame += passed_bonus[1][rrank];
            } else {
                // Candidate passed pawns (one enemy pawn one file away).
                // TODO: this condition could be more sophisticated.
                int blockers = 0;
                for (to = sq + push;
                        pos->board[to] != OUT_OF_BOUNDS; to += push) {
                    if (pos->board[to-1] == opp_pawn) ++blockers;
                    if (pos->board[to] == opp_pawn) blockers = 2;
                    if (pos->board[to+1] == opp_pawn) ++blockers;
                }
                if (blockers < 2) {
                    pd->score[color].midgame += candidate_bonus[0][rrank];
                    pd->score[color].endgame += candidate_bonus[1][rrank];
                }
            }

            // Pawn storm scores.
            pd->kingside_storm[color] += king_storm[sq ^ (0x70*color)];
            pd->queenside_storm[color] += queen_storm[sq ^ (0x70*color)];

            // Isolated pawns.
            bool isolated = !(neighbor_file_mask[file] & our_pawns);
            if (isolated) {
                pd->score[color].midgame -= isolation_penalty[0][file];
                pd->score[color].endgame -= isolation_penalty[1][file];
            }

            // Doubled pawns.
            bool doubled = (in_front_mask[color^1][ind] & our_pawns) != 0;
            if (doubled) {
                pd->score[color].midgame -= doubled_penalty[0][file];
                pd->score[color].endgame -= doubled_penalty[1][file];
            }

            // Connected pawns.
            bool connected = neighbor_file_mask[file] & our_pawns &
                (rank_mask[rank] | rank_mask[rank + (color == WHITE ? 1:-1)]);
            if (connected) {
                pd->score[color].midgame += connected_bonus[0];
                pd->score[color].endgame += connected_bonus[1];
            }
            
            // Backward pawns (unsupportable by pawns, can't advance)
            if (!passed && !isolated && !connected &&
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
                    }
                }
            }
        }
    }
    return pd;
}

/*
 * Retrieve (and calculate if necessary) the pawn data associated with |pos|,
 * and use it to determine the overall pawn score for the given position. The
 * pawn data is also used as an input to other evaluation functions.
 */
score_t pawn_score(const position_t* pos, pawn_data_t** pawn_data)
{
    pawn_data_t* pd = analyze_pawns(pos);
    if (pawn_data) *pawn_data = pd;
    int passer_bonus[2] = {0, 0};
    int storm_score[2] = {0, 0};
    file_t king_file[2] = { square_file(pos->pieces[WHITE][0]),
                            square_file(pos->pieces[BLACK][0]) };
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
        if (king_file[side] < FILE_E && king_file[side^1] > FILE_E) {
            storm_score[side] = pd->kingside_storm[side];
        } else if (king_file[side] > FILE_E && king_file[side^1] < FILE_E) {
            storm_score[side] = pd->queenside_storm[side];
        }
    }

    color_t side = pos->side_to_move;
    score_t score;
    score.midgame = pd->score[side].midgame + passer_bonus[side] -
        (pd->score[side^1].midgame + passer_bonus[side^1]) +
        storm_score[side] - storm_score[side^1];
    score.endgame = pd->score[side].endgame + passer_bonus[side] -
        (pd->score[side^1].endgame + passer_bonus[side^1]);
    return score;
}

