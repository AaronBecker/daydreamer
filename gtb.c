
#include "daydreamer.h"
#include "gtb/gtb-probe.h"
#include <string.h>

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
    return success;
}

void unload_gtb(void)
{
    tbcache_done();
    tb_done();
    tb_paths = tbpaths_done(tb_paths);
}

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

bool probe_gtb_soft(const position_t* pos, int* score)
{
    int stm = stm_to_gtb(pos->side_to_move);
    int ep = square_to_gtb(pos->ep_square);
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

bool probe_gtb_hard(const position_t* pos, int* score)
{
    int stm = stm_to_gtb(pos->side_to_move);
    int ep = square_to_gtb(pos->ep_square);
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
