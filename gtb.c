
#include "daydreamer.h"
#include "gtb/gtb-probe.h"
#include <string.h>
#include <pthread.h>

#define piece_to_gtb(p)     piece_to_gtb_table[p]
#define square_to_gtb(s)    square_to_index(s)
#define ep_to_gtb(s)        (s ? square_to_index(s) : tb_NOSQUARE)
#define stm_to_gtb(s)       (s)
#define castle_to_gtb(c)    castle_to_gtb_table[c]
#define DEFAULT_GTB_CACHE_SIZE  (32*1024*1024)

static char** tb_paths = NULL;
static const int piece_to_gtb_table[] = {
    tb_NOPIECE, tb_PAWN, tb_KNIGHT, tb_BISHOP, tb_ROOK, tb_QUEEN, tb_KING, 0,
    tb_NOPIECE, tb_PAWN, tb_KNIGHT, tb_BISHOP, tb_ROOK, tb_QUEEN, tb_KING, 0
};
static const int castle_to_gtb_table[] = {
    tb_NOCASTLE,
                                 tb_WOO,
                        tb_BOO         ,
                        tb_BOO | tb_WOO,
              tb_WOOO                  ,
              tb_WOOO |          tb_WOO,
              tb_WOOO | tb_BOO         ,
              tb_WOOO | tb_BOO | tb_WOO,
    tb_BOOO                            ,
    tb_BOOO |                    tb_WOO,
    tb_BOOO |           tb_BOO         ,
    tb_BOOO |           tb_BOO | tb_WOO,
    tb_BOOO | tb_WOOO                  ,
    tb_BOOO | tb_WOOO |          tb_WOO,
    tb_BOOO | tb_WOOO | tb_BOO,
    tb_BOOO | tb_WOOO | tb_BOO | tb_WOO,
};

#define GTB_MAX_POOL_SIZE   16
typedef struct {
    int stm, ep, castle;
    unsigned int ws[17], bs[17];
    unsigned char wp[17], bp[17];
    uint8_t _pad[CACHE_LINE_BYTES -
        ((3*sizeof(int) +
          2*17*sizeof(int) +
          2*17*sizeof(char)) % CACHE_LINE_BYTES)];
} gtb_pool_args_t;

static thread_pool_t gtb_pool_storage;
static thread_pool_t* gtb_pool = NULL;
CACHE_ALIGN static thread_info_t gtb_pool_info[GTB_MAX_POOL_SIZE];
CACHE_ALIGN static gtb_pool_args_t gtb_pool_args[GTB_MAX_POOL_SIZE];

void* gtb_probe_firm_worker(void* payload);

/*
 * Given a string identifying the location of Gaviota tb's, load those
 * tb's for use during search.
 */
bool load_gtb(char* gtb_pathlist, int cache_size_bytes)
{
    if (tb_is_initialized()) unload_gtb();
    assert(tb_paths == NULL);
    assert(cache_size_bytes >= 0);
    assert(tb_WHITE_TO_MOVE == WHITE && tb_BLACK_TO_MOVE == BLACK);
    char* path = NULL; 
    tb_paths = tbpaths_init();
    while ((path = strsep(&gtb_pathlist, ";"))) {
        if (!*path) continue;
        tb_paths = tbpaths_add(tb_paths, path);
    }
    int scheme = tb_CP4;
    int verbosity = 0;
    tb_init(verbosity, scheme, tb_paths);
    if (!cache_size_bytes) cache_size_bytes = DEFAULT_GTB_CACHE_SIZE;
    tbcache_init(cache_size_bytes);
    tbstats_reset();
    bool success = tb_is_initialized() && tbcache_is_on();
    if (success) {
        if (options.verbose) {
            printf("info string loaded Gaviota TBs with a pool of %d threads\n",
                    options.eg_pool_threads);
        }
        gtb_pool = &gtb_pool_storage;
        init_thread_pool(gtb_pool, gtb_pool_args, gtb_pool_info,
            sizeof(gtb_pool_args_t), options.eg_pool_threads);
    } else if (options.verbose) {
        printf("info string failed to load Gaviota TBs\n");
    }
    return success;
}

/*
 * Unload all tablebases and destroy the thread pool used for probing
 * during search.
 */
void unload_gtb(void)
{
    if (!tb_is_initialized()) return;
    tbcache_done();
    tb_done();
    tb_paths = tbpaths_done(tb_paths);
    if (gtb_pool) {
        destroy_thread_pool(gtb_pool);
        gtb_pool = NULL;
    }
}

/*
 * Fill arrays with the information needed by the gtb probing code.
 */
static void fill_gtb_arrays(const position_t* pos,
        unsigned int* ws,
        unsigned int* bs,
        unsigned char* wp,
        unsigned char* bp)
{
    square_t sq;
    int count = 0, i;
    for (i=0; i<pos->num_pieces[WHITE]; ++i) {
        sq = pos->pieces[WHITE][i];
        ws[count] = square_to_gtb(sq);
        wp[count++] = piece_to_gtb(pos->board[sq]);
    }
    for (i=0; i<pos->num_pawns[WHITE]; ++i) {
        sq = pos->pawns[WHITE][i];
        ws[count] = square_to_gtb(sq);
        wp[count++] = piece_to_gtb(pos->board[sq]);
    }
    ws[count] = tb_NOSQUARE;
    wp[count] = tb_NOPIECE;

    count = 0;
    for (i=0; i<pos->num_pieces[BLACK]; ++i) {
        sq = pos->pieces[BLACK][i];
        bs[count] = square_to_gtb(sq);
        bp[count++] = piece_to_gtb(pos->board[sq]);
    }
    for (i=0; i<pos->num_pawns[BLACK]; ++i) {
        sq = pos->pawns[BLACK][i];
        bs[count] = square_to_gtb(sq);
        bp[count++] = piece_to_gtb(pos->board[sq]);
    }
    bs[count] = tb_NOSQUARE;
    bp[count] = tb_NOPIECE;
}

/*
 * Probe the tb cache only, without considering what's available on disk.
 */
bool probe_gtb_soft(const position_t* pos, int* score)
{
    int stm = stm_to_gtb(pos->side_to_move);
    int ep = ep_to_gtb(pos->ep_square);
    int castle = castle_to_gtb(pos->castle_rights);

    unsigned int ws[17], bs[17];
    unsigned char wp[17], bp[17];
    fill_gtb_arrays(pos, ws, bs, wp, bp);

    unsigned res, val;
    int success = tb_probe_soft(stm, ep, castle, ws, bs, wp, bp, &res, &val);
    if (success) {
        if (res == tb_DRAW) *score = DRAW_VALUE;
        else if (res == tb_BMATE) *score = mated_in(val);
        else if (res == tb_WMATE) *score = mate_in(val);
        else assert(false);
        if (pos->side_to_move == BLACK) *score *= -1;
    }
    return success;
}

/*
 * Probe all tbs, using the cache if available, but blocking and waiting for
 * disk if we miss in cache.
 */
bool probe_gtb_hard(const position_t* pos, int* score)
{
    int stm = stm_to_gtb(pos->side_to_move);
    int ep = ep_to_gtb(pos->ep_square);
    int castle = castle_to_gtb(pos->castle_rights);

    unsigned int ws[17], bs[17];
    unsigned char wp[17], bp[17];
    fill_gtb_arrays(pos, ws, bs, wp, bp);

    unsigned res, val;
    int success = tb_probe_hard(stm, ep, castle, ws, bs, wp, bp, &res, &val);
    if (success) {
        if (res == tb_DRAW) *score = DRAW_VALUE;
        else if (res == tb_BMATE) *score = mated_in(val);
        else if (res == tb_WMATE) *score = mate_in(val);
        else assert(false);
        if (pos->side_to_move == BLACK) *score *= -1;
    }
    return success;
}

/*
 * A compromise between probe_hard and probe_soft. Check the cache, and return
 * if the position is found. If not, use a thread to load the position into
 * cache in the background while we return to the main search. The background
 * load uses a thread pool, and has minimal load implications because the
 * worker threads are blocked nearly 100% of the time.
 */
bool probe_gtb_firm(const position_t* pos, int* score)
{
    int pool_slot;
    gtb_pool_args_t* gtb_args;
    void* slot_addr;
    bool available_slot = get_slot(gtb_pool, &pool_slot, &slot_addr);
    if (!available_slot) {
        gtb_pool_args_t args_storage;
        gtb_args = &args_storage;
    } else {
        gtb_args = (gtb_pool_args_t*)slot_addr;
    }

    gtb_args->stm = stm_to_gtb(pos->side_to_move);
    gtb_args->ep = ep_to_gtb(pos->ep_square);
    gtb_args->castle = castle_to_gtb(pos->castle_rights);

    fill_gtb_arrays(pos, gtb_args->ws, gtb_args->bs,
            gtb_args->wp, gtb_args->bp);

    unsigned res, val;
    int success = tb_probe_soft(gtb_args->stm, gtb_args->ep, gtb_args->castle,
            gtb_args->ws, gtb_args->bs, gtb_args->wp, gtb_args->bp, &res, &val);
    if (success) {
        if (res == tb_DRAW) *score = DRAW_VALUE;
        else if (res == tb_BMATE) *score = mated_in(val);
        else if (res == tb_WMATE) *score = mate_in(val);
        else assert(false);
        if (pos->side_to_move == BLACK) *score *= -1;
        return true;
    }

    if (available_slot) run_thread(gtb_pool, gtb_probe_firm_worker, pool_slot);
    return false;
}

/*
 * The worker function for background probing in probe_firm.
 */
void* gtb_probe_firm_worker(void* payload)
{
    gtb_pool_args_t* gtb_args = (gtb_pool_args_t*)payload;
    unsigned res, val;
    int success = tb_probe_hard(gtb_args->stm,
            gtb_args->ep,
            gtb_args->castle,
            gtb_args->ws,
            gtb_args->bs,
            gtb_args->wp,
            gtb_args->bp,
            &res,
            &val);
    (void)success;
    return NULL;
}
