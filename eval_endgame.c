
#include "daydreamer.h"

static void scale_kpk(const position_t* pos, eval_data_t* ed, int scale[2]);

eg_scale_fn eg_scale_fns[] = {
    NULL,           //EG_NONE,
    NULL,           //EG_WIN,
    NULL,           //EG_LOSS,
    NULL,           //EG_DRAW,
    NULL,           //EG_KQKQ,
    NULL,           //EG_KQKP,
    NULL,           //EG_KRKR,
    NULL,           //EG_KRKB,
    NULL,           //EG_KRKN,
    NULL,           //EG_KRKP,
    NULL,           //EG_KRPKR,
    NULL,           //EG_KRPPKRP,
    NULL,           //EG_KBBKN,
    NULL,           //EG_KBPKB,
    NULL,           //EG_KBPKN,
    NULL,           //EG_KBKP,
    NULL,           //EG_KBPPKB,
    NULL,           //EG_KNPK,
    &scale_kpk,     //EG_KPK,
    NULL,           //EG_LAST
};

void determine_endgame_scale(const position_t* pos,
        eval_data_t* ed,
        int endgame_scale[2])
{
    endgame_scale[WHITE] = ed->md->scale[WHITE];
    endgame_scale[BLACK] = ed->md->scale[BLACK];
    eg_scale_fn fn = eg_scale_fns[ed->md->eg_type];
    if (fn) fn(pos, ed, endgame_scale);
}

static void scale_kpk(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    bool sstm = pos->side_to_move == strong_side;
    assert(pos->num_pieces[strong_side] == 1);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[weak_side] == 1);
    assert(pos->num_pawns[weak_side] == 0);

    square_t sk, wk, p;
    p = pos->pawns[strong_side][0];
    if (square_file(p) < FILE_E) {
        sk = pos->pieces[strong_side][0];
        wk = pos->pieces[weak_side][0];
    } else {
        sk = mirror_file(pos->pieces[strong_side][0]);
        wk = mirror_file(pos->pieces[weak_side][0]);
        p = mirror_file(p);
    }

    square_t push = pawn_push[strong_side];
    rank_t p_rank = relative_rank[strong_side][square_rank(p)];
    bool draw = false;
    if (wk == p + push) {
        if (p_rank <= RANK_6) {
            draw = true;
        } else {
            if (sstm) {
                if (sk == p-push-1 || sk == p-push+1) draw = true;
            } else if (sk != p-push-1 && sk != p-push+1) draw = true;
        }
    } else if (wk == p + 2*push) {
        if (p_rank <= RANK_5) draw = true;
        else {
            assert(p_rank == RANK_6);
            if (!sstm || (sk != p-1 && sk != p+1)) draw = true;
        }
    } else if (sk == p-1 || sk == p+1) {
        if (wk == sk + 2*push && sstm) draw = true;
    } else if (sk >= p+push-1 && sk <= p+push+1) {
        if (p_rank <= RANK_4 && wk == sk + 2*push && sstm) draw = true;
    }

    if (!draw && square_file(p) == FILE_A) {
        if (distance(wk, strong_side == WHITE ? A8 : A1) <= 1) draw = true;
        else if (square_file(sk) == FILE_A &&
                square_file(wk) == FILE_C &&
                    relative_rank[strong_side][square_rank(wk)] >
                    p_rank + (p_rank == RANK_2)) draw = true;
    }

    if (draw) scale[0] = scale[1] = 0;
}

