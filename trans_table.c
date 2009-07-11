
#include <assert.h>
#include <stdlib.h>
#include "daydreamer.h"

static int transposition_table_size;
static const int bucket_size = 4;
static int num_buckets;
transposition_entry_t* transposition_table = NULL;

void init_transposition_table(int max_bytes)
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
}

transposition_entry_t* get_transposition(position_t* pos)
{
}

void put_transposition(position_t* pos, int depth, int score)
{
}
