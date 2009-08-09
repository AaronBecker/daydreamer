
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "daydreamer.h"

static const int bucket_size = 4;
static int num_buckets;
static int generation;
static const int generation_limit = 8;
static int age_score_table[8];
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

#define entry_replace_score(entry) \
    (age_score_table[(entry)->age] - (entry)->depth)

static void set_transposition_age(int age);

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
    set_transposition_age(0);
}

void clear_transposition_table(void)
{
    memset(transposition_table, 0,
            sizeof(transposition_entry_t)*bucket_size*num_buckets);
    memset(&hash_stats, 0, sizeof(hash_stats));
}

static void set_transposition_age(int age)
{
    assert(age >= 0 && age < generation_limit);
    generation = age;
    for (int i=0; i<generation_limit; ++i) {
        int age = generation - i;
        if (age < 0) age += generation_limit;
        age_score_table[i] = age * 128;
    }
}

void increment_transposition_age(void)
{
    set_transposition_age((generation + 1) % generation_limit);
}

transposition_entry_t* get_transposition_entry(position_t* pos)
{
    transposition_entry_t* entry;
    entry = &transposition_table[(pos->hash % num_buckets) * bucket_size];
    for (int i=0; i<bucket_size; ++i, ++entry) {
        if (!entry->key || entry->key != pos->hash) continue;
        hash_stats.hits++;
        return entry;
    }
    hash_stats.misses++;
    return NULL;
}

bool get_transposition(position_t* pos,
        int depth,
        int* lb,
        int* ub,
        move_t* move)
{
    transposition_entry_t* entry;
    entry = &transposition_table[(pos->hash % num_buckets) * bucket_size];
    for (int i=0; i<bucket_size; ++i, ++entry) {
        if (!entry->key || entry->key != pos->hash) continue;
        // found it
        entry->age = generation;
        hash_stats.hits++;
        *move = entry->move;
        if (depth <= entry->depth) {
            if (entry->score_type != SCORE_LOWERBOUND &&
                    entry->score < *ub) *ub = entry->score;
            if (entry->score_type != SCORE_UPPERBOUND &&
                    entry->score > *lb) *lb = entry->score;
        }
        return true;
    }
    hash_stats.misses++;
    return false;
}

void put_transposition(position_t* pos,
        move_t move,
        int depth,
        int score,
        score_type_t score_type)
{
    transposition_entry_t* entry, *best_entry = NULL;
    int replace_score, best_replace_score = INT_MIN;
    entry = &transposition_table[(pos->hash % num_buckets) * bucket_size];
    for (int i=0; i<bucket_size; ++i, ++entry) {
        if (entry->key == pos->hash) {
            // Update an existing entry
            entry->age = generation;
            entry->depth = depth;
            entry->move = move;
            entry->score = score;
            switch (score_type) {
                case SCORE_LOWERBOUND: hash_stats.beta++; break;
                case SCORE_UPPERBOUND: hash_stats.alpha++; break;
                case SCORE_EXACT: hash_stats.exact++;
            }
            switch (entry->score_type) {
                case SCORE_LOWERBOUND: hash_stats.beta--; break;
                case SCORE_UPPERBOUND: hash_stats.alpha--; break;
                case SCORE_EXACT: hash_stats.exact--;
            }
            entry->score_type = score_type;
            return;
        }
        replace_score = entry_replace_score(entry);
        if (replace_score > best_replace_score) {
            best_entry = entry;
            best_replace_score = replace_score;
        }
    }
    // Replace the entry with the highest replace score.
    assert(best_entry != NULL);
    entry = best_entry;
    if (!entry->key || entry->age != generation) hash_stats.occupied++;
    else ++hash_stats.evictions;
    switch (score_type) {
        case SCORE_LOWERBOUND: hash_stats.beta++; break;
        case SCORE_UPPERBOUND: hash_stats.alpha++; break;
        case SCORE_EXACT: hash_stats.exact++;
    }
    entry->age = generation;
    entry->key = pos->hash;
    entry->move = move;
    entry->depth = depth;
    entry->score = score;
    entry->score_type = score_type;
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
