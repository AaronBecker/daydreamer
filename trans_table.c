
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "daydreamer.h"

static const int bucket_size = 1;
static int num_buckets;
transposition_entry_t* transposition_table = NULL;

static struct {
    int misses;
    int hits;
    int occupied;
    int alpha;
    int beta;
    int exact;
    int evictions;
    int collisions;
} hash_stats;

void init_transposition_table(const int max_bytes)
{
    assert(max_bytes >= 1024);
    int size = sizeof(transposition_entry_t) * bucket_size;
    num_buckets = 1;
    while (size <= max_bytes >> 1) {
        size <<= 1;
        num_buckets <<= 1;
    }
    if (transposition_table) free(transposition_table);
    transposition_table = malloc(size);
    assert(transposition_table);
    clear_transposition_table();
}

void clear_transposition_table(void)
{
    memset(transposition_table, 0, sizeof(transposition_entry_t)*bucket_size);
    memset(&hash_stats, 0, sizeof(hash_stats));
}

bool get_transposition(position_t* pos,
        int depth,
        int* lb,
        int* ub,
        move_t* move)
{
    transposition_entry_t* entry;
    entry = &transposition_table[(pos->hash % num_buckets) * bucket_size];
    if (!entry->key || entry->key != pos->hash) {
        hash_stats.misses++;
        return false;
    }
    hash_stats.hits++;
    *move = entry->move;
    if (depth <= entry->depth) {
        if ((entry->score_type == SCORE_UPPERBOUND ||
                entry->score_type == SCORE_EXACT) &&
                entry->score < *ub) *ub = entry->score;
        if ((entry->score_type == SCORE_LOWERBOUND ||
                entry->score_type == SCORE_EXACT) &&
                entry->score > *lb) *lb = entry->score;
    }
    return true;
}

void put_transposition(position_t* pos,
        move_t move,
        int depth,
        int score,
        score_type_t score_type)
{
    transposition_entry_t* entry;
    entry = &transposition_table[(pos->hash % num_buckets) * bucket_size];
    if (!entry->key) hash_stats.occupied++;
    else if (entry->key != pos->hash) ++hash_stats.evictions;
    // simple, always-replace policy for now; ignore the buckets
    entry->key = pos->hash;
    entry->move = move;
    entry->depth = depth;
    entry->score = score;
    entry->score_type = score_type;
    switch (score_type) {
        case SCORE_LOWERBOUND: hash_stats.beta++; break;
        case SCORE_UPPERBOUND: hash_stats.alpha++; break;
        case SCORE_EXACT: hash_stats.exact++;
    }
}

void print_transposition_stats(void)
{
    printf("info string hash entries %d", num_buckets);
    printf(" filled: %d (%.2f%%)", hash_stats.occupied,
            (float)hash_stats.occupied / (float)num_buckets * 100.);
    printf(" evictions: %d", hash_stats.evictions);
    printf(" hits: %d (%.2f%%)", hash_stats.hits,
            (float)hash_stats.hits / (hash_stats.hits+hash_stats.misses)*100.);
    printf(" misses: %d (%.2f%%)", hash_stats.misses,
            (float)hash_stats.misses/(hash_stats.hits+hash_stats.misses)*100.);
    printf(" alpha: %d", hash_stats.alpha);
    printf(" beta: %d", hash_stats.beta);
    printf(" exact: %d\n", hash_stats.exact);
}
