
#include "daydreamer.h"
#include <string.h>

static material_data_t* material_table = NULL;
static void compute_material_data(const position_t* pos, material_data_t* md);

static int num_buckets;
static struct {
    int misses;
    int hits;
    int occupied;
    int evictions;
} material_hash_stats;

/*
 * Create a material hash table of the appropriate size.
 */
void init_material_table(const int max_bytes)
{
    assert(max_bytes >= 1024);
    int size = sizeof(material_data_t);
    num_buckets = 1;
    while (size <= max_bytes >> 1) {
        size <<= 1;
        num_buckets <<= 1;
    }
    if (material_table != NULL) free(material_table);
    material_table = malloc(size);
    assert(material_table);
    clear_material_table();
}

/*
 * Wipe the entire table.
 */
void clear_material_table(void)
{
    memset(material_table, 0, sizeof(material_data_t) * num_buckets);
    memset(&material_hash_stats, 0, sizeof(material_hash_stats));
}

/*
 * Look up the material data for the given position.
 */
material_data_t* get_material_data(const position_t* pos)
{
    material_data_t* md = &material_table[pos->material_hash % num_buckets];
    if (md->key == pos->material_hash) {
        material_hash_stats.hits++;
        return md;
    } else if (md->key != 0) {
        material_hash_stats.evictions++;
    } else {
        material_hash_stats.misses++;
        material_hash_stats.occupied++;
    }
    compute_material_data(pos, md);
    md->key = pos->material_hash;
    return md;
}

static void compute_material_data(const position_t* pos, material_data_t* md)
{
    md->phase = game_phase(pos);

    md->score.midgame = 0;
    md->score.endgame = 0;

    int wp = pos->piece_count[WP];
    int bp = pos->piece_count[BP];
    int wn = pos->piece_count[WN];
    int bn = pos->piece_count[BN];
    int wb = pos->piece_count[WB];
    int bb = pos->piece_count[BB];
    int wr = pos->piece_count[WR];
    int br = pos->piece_count[BR];
    int wq = pos->piece_count[WQ];
    int bq = pos->piece_count[BQ];
    int w_major = 2*wq + wr;
    int w_minor = wn + wb;
    int w_piece = 2*w_major + w_minor;
    int w_all = wq + wr + wb + wn + wp;
    int b_major = 2*bq + br;
    int b_minor = bn + bb;
    int b_piece = 2*b_major + b_minor;
    int b_all = bq + br + bb + bn + bp;

    // Pair bonuses
    if (wb > 1) {
        md->score.midgame += 30;
        md->score.endgame += 45;
    }
    if (bb > 1) {
        md->score.midgame -= 30;
        md->score.endgame -= 45;
    }
    if (wr > 1) {
        md->score.midgame -= 12;
        md->score.endgame -= 17;
    }
    if (br > 1) {
        md->score.midgame += 12;
        md->score.endgame += 17;
    }
    if (wq > 1) {
        md->score.midgame -= 8;
        md->score.endgame -= 12;
    }
    if (bq > 1) {
        md->score.midgame += 8;
        md->score.endgame += 12;
    }

    // Pawn bonuses
    int material_adjust = 0;
    material_adjust += wn * 3 * (wp - 4);
    material_adjust -= bn * 3 * (bp - 4);
    material_adjust += wb * 2 * (wp - 4);
    material_adjust -= bb * 2 * (bp - 4);
    material_adjust += wr * (-3) * (wp - 4);
    material_adjust -= br * (-3) * (bp - 4);
    md->score.midgame += material_adjust;
    md->score.endgame += material_adjust;

    // Recognize specific material combinations where we want to do separate
    // scaling or scoring.
    md->eg_type = EG_NONE;
    if (w_all + b_all == 0) {
        md->eg_type = EG_DRAW;
    } else if (w_all + b_all == 1) {
        if (wp) {
            md->eg_type = EG_KPK;
            md->strong_side = WHITE;
        } else if (bp) {
            md->eg_type = EG_KPK;
            md->strong_side = BLACK;
        } else if (!wq  && !wr && !bq && !br) md->eg_type = EG_DRAW;
    } else if (w_all == 1 && b_all == 1) {
        if (wq && bq) {
            md->eg_type = EG_KQKQ;
        } else if (wq && bp) {
            md->eg_type = EG_KQKP;
            md->strong_side = WHITE;
        } else if (bq && wp) {
            md->eg_type = EG_KQKP;
            md->strong_side = BLACK;
        } else if (wr && br) {
            md->eg_type = EG_KRKR;
        } else if (wr && bb) {
            md->eg_type = EG_KRKB;
            md->strong_side = WHITE;
        } else if (br && wb) {
            md->eg_type = EG_KRKB;
            md->strong_side = BLACK;
        } else if (wr && bn) {
            md->eg_type = EG_KRKN;
            md->strong_side = WHITE;
        } else if (br && wn) {
            md->eg_type = EG_KRKN;
            md->strong_side = BLACK;
        } else if (wr && bp) {
            md->eg_type = EG_KRKP;
            md->strong_side = WHITE;
        } else if (br && wp) {
            md->eg_type = EG_KRKP;
            md->strong_side = BLACK;
        } else if (wb && bp) {
            md->eg_type = EG_KBKP;
            md->strong_side = BLACK;
        } else if (bb && wp) {
            md->eg_type = EG_KBKP;
            md->strong_side = WHITE;
        } else if (wn && wp) {
            md->eg_type = EG_KNPK;
            md->strong_side = WHITE;
        } else if (bn && bp) {
            md->eg_type = EG_KNPK;
            md->strong_side = BLACK;
        }
    } else if (w_all + b_all == 3) {
        if (wr == 1 && br == 1 && wp == 1) {
            md->eg_type = EG_KRPKR;
            md->strong_side = WHITE;
        } else if (wr == 1 && br == 1 && bp == 1) {
            md->eg_type = EG_KRPKR;
            md->strong_side = BLACK;
        } else if (wb == 2 && bn == 1) {
            md->eg_type = EG_KBBKN;
            md->strong_side = WHITE;
        } else if (bb == 2 && wn == 1) {
            md->eg_type = EG_KBBKN;
            md->strong_side = BLACK;
        } else if (wb == 1 && wp == 1 && bb == 1) {
            md->eg_type = EG_KBPKB;
            md->strong_side = WHITE;
        } else if (wb == 1 && bp == 1 && bb == 1) {
            md->eg_type = EG_KBPKB;
            md->strong_side = BLACK;
        } else if (wb == 1 && wp == 1 && bn == 1) {
            md->eg_type = EG_KBPKN;
            md->strong_side = WHITE;
        } else if (bb == 1 && bp == 1 && wn == 1) {
            md->eg_type = EG_KBPKN;
            md->strong_side = BLACK;
        }
    } else if (wb == 1 && wp == 2 && bb == 1) {
        md->eg_type = EG_KBPPKB;
        md->strong_side = WHITE;
    } else if (bb == 1 && bp == 2 && wb == 1) {
        md->eg_type = EG_KBPPKB;
        md->strong_side = BLACK;
    } else if (wr == 1 && wp == 2 && br == 1 && bp == 1) {
        md->eg_type = EG_KRPPKRP;
        md->strong_side = WHITE;
    } else if (br == 1 && bp == 2 && wr == 1 && wp == 1) {
        md->eg_type = EG_KRPPKRP;
        md->strong_side = BLACK;
    }

    // Endgame scaling factors
    md->scale[WHITE] = md->scale[BLACK] = 16;
    if (md->eg_type == EG_KQKQ || md->eg_type == EG_KRKR) {
        md->scale[BLACK] = md-->scale[WHITE] = 0;
        return;
    }

    if (!wp) {
        if (w_piece == 1) {
            md->scale[WHITE] = 0;
        } else if (w_piece == 2 && wn == 2) {
            if (b_piece != 0 || bp == 0) {
                md->scale[WHITE] = 0;
            } else {
                md->scale[WHITE] = 1;
            }
        } else if (w_piece == 2 && wb == 2 && b_piece == 1 && bn == 1) {
            md->scale[WHITE] = 8;
        } else if (w_piece - b_piece <= 1 && w_major <= 2) {
            md->scale[WHITE] = 2;
        }
    } else if (wp == 1) {
        if (b_minor != 0) {
            if (w_piece == 1) {
                md->scale[WHITE] = 4;
            } else if (w_piece == 2 && wn == 2) {
                md->scale[WHITE] = 4;
            } else if (w_piece - b_piece <= 2 && w_major <= 2) {
                md->scale[WHITE] = 8;
            }
        } else if (br) {
            if (w_piece == 1) {
                md->scale[WHITE] = 4;
            } else if (w_piece == 2 && wn == 2) {
                md->scale[WHITE] = 4;
            } else if (w_piece - b_piece <= -1 && w_major <= 2) {
                md->scale[WHITE] = 8;
            }
        }
    }

    if (!bp) {
        if (b_piece == 1) {
            md->scale[BLACK] = 0;
        } else if (b_piece == 2 && bn == 2) {
            if (w_piece != 0 || wp == 0) {
                md->scale[BLACK] = 0;
            } else {
                md->scale[BLACK] = 1;
            }
        } else if (b_piece == 2 && bb == 2 && w_piece == 1 && wn == 1) {
            md->scale[BLACK] = 8;
        } else if (b_piece - w_piece <= 1 && b_major <= 2) {
            md->scale[BLACK] = 2;
        }
    } else if (bp == 1) {
        if (w_minor != 0) {
            if (b_piece == 1) {
                md->scale[BLACK] = 4;
            } else if (b_piece == 2 && bn == 2) {
                md->scale[BLACK] = 4;
            } else if (b_piece - w_piece <= -1 && b_major <= 2) {
                md->scale[BLACK] = 8;
            }
        } else if (wr) {
            if (b_piece == 1) {
                md->scale[BLACK] = 4;
            } else if (b_piece == 2 && bn == 2) {
                md->scale[BLACK] = 4;
            } else if (b_piece - w_piece <= 3 && b_major <= 2) {
                md->scale[BLACK] = 8;
            }
        }
    }
}

