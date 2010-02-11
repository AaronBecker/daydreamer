
#include "daydreamer.h"

static void scale_krkp(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_krpkr(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_knpk(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_kpkb(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_kbpk(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_kpk(const position_t* pos, eval_data_t* ed, int scale[2]);

static int score_win(const position_t* pos, eval_data_t* ed);
static int score_draw(const position_t* pos, eval_data_t* ed);
static int score_krpkr(const position_t* pos, eval_data_t* ed);
static int score_kbnk(const position_t* pos, eval_data_t* ed);

eg_scale_fn eg_scale_fns[] = {
    NULL,           //EG_NONE,
    NULL,           //EG_WIN,
    NULL,           //EG_DRAW,
    NULL,           //EG_KQKQ,
    NULL,           //EG_KQKP,
    NULL,           //EG_KRKR,
    NULL,           //EG_KRKB,
    NULL,           //EG_KRKN,
    &scale_krkp,    //EG_KRKP,
    NULL,//&scale_krpkr,   //EG_KRPKR,
    NULL,           //EG_KRPPKRP,
    NULL,           //EG_KBBKN,
    NULL,           //EG_KBNK,
    NULL,           //EG_KBPKB,
    NULL,           //EG_KBPKN,
    NULL,//&scale_kpkb,    //EG_KPKB,
    NULL,           //EG_KBPPKB,
    &scale_knpk,    //EG_KNPK,
    &scale_kbpk,    //EG_KBPK,
    &scale_kpk,     //EG_KPK,
    NULL,           //EG_LAST
};

eg_score_fn eg_score_fns[] = {
    NULL,           //EG_NONE,
    &score_win,     //EG_WIN,
    &score_draw,    //EG_DRAW,
    NULL,           //EG_KQKQ,
    NULL,           //EG_KQKP,
    NULL,           //EG_KRKR,
    NULL,           //EG_KRKB,
    NULL,           //EG_KRKN,
    NULL,           //EG_KRKP,
    &score_krpkr,   //EG_KRPKR,
    NULL,           //EG_KRPPKRP,
    NULL,           //EG_KBBKN,
    &score_kbnk,    //EG_KBNK,
    NULL,           //EG_KBPKB,
    NULL,           //EG_KBPKN,
    NULL,           //EG_KPKB,
    NULL,           //EG_KBPPKB,
    NULL,           //EG_KNPK,
    NULL,           //EG_KBPK,
    NULL,           //EG_KPK,
    NULL,           //EG_LAST
};

bool endgame_score(const position_t* pos, eval_data_t* ed, int* score)
{
    if (options.use_gtb && ed->md->eg_type == EG_KPK) {
        int result;
        bool hit = probe_gtb_soft(pos, &result);
        if (hit) {
            *score  = result * (pos->side_to_move == WHITE ? 1 : -1);
            return true;
        }
    }
    return false;
    /*
    eg_score_fn fn = eg_score_fns[ed->md->eg_type];
    if (fn) {
        *score = fn(pos, ed);
        return true;
    }
    return false;
    */
}

void determine_endgame_scale(const position_t* pos,
        eval_data_t* ed,
        int endgame_scale[2])
{
    endgame_scale[WHITE] = ed->md->scale[WHITE];
    endgame_scale[BLACK] = ed->md->scale[BLACK];
    eg_scale_fn fn = eg_scale_fns[ed->md->eg_type];
    if (fn) fn(pos, ed, endgame_scale);
}

static void scale_krkp(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 0);
    assert(pos->num_pieces[weak_side] == 1);
    assert(pos->num_pawns[weak_side] == 1);

    square_t bp = pos->pawns[weak_side][0];
    square_t wr = pos->pieces[strong_side][1];
    square_t wk = pos->pieces[strong_side][0];
    square_t bk = pos->pieces[weak_side][0];
    square_t prom_sq = square_file(bp);
    int tempo = pos->side_to_move == strong_side ? 1 : 0;

    if (strong_side == BLACK) {
        wr = mirror_rank(wr);
        wk = mirror_rank(wk);
        bk = mirror_rank(bk);
        bp = mirror_rank(bp);
    }

    if ((wk < bp && square_file(wk) == prom_sq) ||
            (distance(wk, prom_sq) + 1 - tempo < distance(bk, prom_sq)) ||
            (distance(bk, bp) - (tempo^1) >= 3 && distance(bk, wr) >= 3)) {
        scale[strong_side] = 16;
        scale[weak_side] = 0;
        return;
    }
    int dist = MAX(1, distance(bk, prom_sq)) + distance(bp, prom_sq);
    if (bk == bp + S) {
        if (prom_sq == A1 || prom_sq == H1) return;
        dist++;
    }
    if (square_file(wr)!=square_file(bp) && square_rank(wr)!=RANK_1) dist--;
    if (!tempo) dist--;
    if (distance(wk, prom_sq) > dist) {
        scale[0] = scale[1] = 0;
    }
}

static void scale_krpkr(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[weak_side] == 2);
    assert(pos->num_pawns[weak_side] == 0);

    square_t wk = pos->pieces[strong_side][0];
    square_t bk = pos->pieces[weak_side][0];
    square_t wr = pos->pieces[strong_side][1];
    square_t br = pos->pieces[weak_side][1];
    square_t wp = pos->pawns[strong_side][0];
    bool tempo = strong_side == pos->side_to_move;
 
    if (strong_side == BLACK) {
        wk = mirror_rank(wk);
        bk = mirror_rank(bk);
        wr = mirror_rank(wr);
        br = mirror_rank(br);
        wp = mirror_rank(wp);
    }
    if (square_file(wp) >= FILE_E) {
        wk = mirror_file(wk);
        bk = mirror_file(bk);
        wr = mirror_file(wr);
        br = mirror_file(br);
        wp = mirror_file(wp);
    }

    int wk_rank = square_rank(wk);
    int br_rank = square_rank(br);
    int wr_rank = square_rank(wr);
    int p_rank = square_rank(wp);
    int p_file = square_file(wp);
    int prom_sq = p_file + A8;

    // Lots of tricky rook endgames. Credit goes to Silman's Complete Endgame
    // Course (great book), checked against Stockfish's implementation.
    // I'm horrible at rook endgames over the board, so yeah.

    // The Philidor position is a draw.
    if (p_rank < RANK_6 && distance(bk, prom_sq) <= 1 && wk_rank < RANK_6 &&
            (((p_rank < RANK_4 && wr_rank != RANK_6)) ||
            br_rank == RANK_6)) {
        scale[0] = scale[1] = 0;
        return;
    }

    // Philidor defense after white pushes the pawn. Black needs to check
    // the white king from behind.
    int br_file = square_file(br);
    if (p_rank >= RANK_6 && distance(bk, prom_sq) <= 1 &&
            wk_rank + tempo <= RANK_6 &&
            (br_rank == RANK_1 || (abs(br_file - p_file) > 2))) {
        scale[0] = scale[1] = 0;
        return;
    }
    if (p_rank >= RANK_6 && bk == prom_sq && br_rank == RANK_1 &&
            (!tempo || distance(wk, wp) > 1)) {
        scale[0] = scale[1] = 0;
        return;
    }

    // Fiddly rook-pawn promotion blocked by friendly rook. G7 and H7 are
    // the only places the black king is safe.
    int wk_file = square_file(wk);
    if (wp == A7 && wr == A8 && (bk == G7 || bk == H7) &&
            (br_file == FILE_A && (br_rank < RANK_4 ||
                                   wk_file > FILE_C ||
                                   wk_rank < RANK_6))) {
        scale[0] = scale[1] = 0;
        return;
    }

    // Blockaded pawn with the white king far away is a draw.
    if (p_rank < RANK_6 && bk == wp + N &&
            distance(wk, wp) - tempo > 1 &&
            distance(wk, br) - tempo > 1) {
        scale[0] = scale[1] = 0;
        return;
    }

    // Generic supported pawn on 7 usually wins if the king is close.
    int wr_file = square_file(wr);
    if (p_rank == RANK_7 && p_file != FILE_A &&
            wr_file == p_file && wr != prom_sq &&
            distance(wk, prom_sq) < distance(bk, prom_sq) - 2 + tempo &&
            distance(wk, prom_sq) < distance(bk, wr + tempo)) {
        scale[strong_side] = 16 - distance(wp, prom_sq);
        scale[weak_side] = 1;
        return;
    }

    // Generic less-advanced pawn, less favorable.
    if (p_file != FILE_A && wr_file == p_file && wr < wp &&
            distance(wk, prom_sq) < distance(bk, prom_sq) - 2 + tempo &&
            distance(wk, wp + N) < distance(bk, wp + N) - 2 + tempo &&
            (distance(bk, wr) + tempo > 2 ||
             (distance(wk, prom_sq) < distance(bk, wr) + tempo &&
              distance(wk, wp + N) < distance(bk, wr) + tempo))) {
        scale[strong_side] = 16 - distance(wp, prom_sq);
        scale[weak_side] = 0;
        return;
    }

    // Blocked, unsupported pawn; usually a draw.
    int bk_file = square_file(bk);
    if (p_rank < RANK_5 && bk > wp) {
        if (bk_file == p_file) {
            scale[strong_side] = 2;
            scale[weak_side] = 1;
            return;
        } else if (abs(bk_file - p_file) == 1 && distance(wk, bk) > 2) {
            scale[strong_side] = 3;
            scale[weak_side] = 1;
            return;
        }
    }
}

static void scale_kpkb(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 1);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[weak_side] == 2);
    assert(pos->num_pawns[weak_side] == 0);

    square_t wp = pos->pawns[strong_side][0];
    square_t wk = pos->pieces[strong_side][0];
    square_t bk = pos->pieces[weak_side][0];
    square_t bb = pos->pieces[weak_side][1];
    square_t prom_sq = square_file(wp) + A8;

    if (strong_side == BLACK) {
        wp = mirror_rank(wp);
        wk = mirror_rank(wk);
        bk = mirror_rank(bk);
        bb = mirror_rank(bb);
    }

    for (square_t to = wp+N; to != prom_sq; to += N) {
        if (to == bb) {
            scale[0] = scale[1] = 0;
            return;
        }

        if (possible_attack(bb, to, WB)) {
            int dir = direction(bb, to);
            square_t sq;
            for (sq=bb+dir; sq != to && sq != bk; sq+=dir) {}
            if (sq == to) scale[0] = scale[1] = 0;
            return;
        }
    }
}

static void scale_knpk(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[strong_side^1] == 0);
    assert(pos->num_pawns[strong_side^1] == 0);

    square_t p = pos->pawns[strong_side][0];
    square_t wk = pos->pieces[strong_side^1][0];
    if (strong_side == BLACK) {
        wk = mirror_rank(wk);
        p = mirror_rank(p);
    }
    if (square_file(p) == FILE_H) {
        wk = mirror_file(wk);
        p = mirror_file(p);
    }
    if (p == A7 && distance(wk, A8) <= 1) {
        scale[0] = scale[1] = 0;
    }
}

static void scale_kbpk(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[strong_side^1] == 1);
    assert(pos->num_pawns[strong_side^1] == 0);

    file_t pf = square_file(pos->pawns[strong_side][0]);
    color_t bc = square_color(pos->pieces[strong_side][1]);
    if (pf == FILE_H) {
        pf = FILE_A;
        bc ^= 1;
    }

    if (pf == FILE_A &&
            distance(pos->pieces[strong_side^1][0], A8*(strong_side^1)) <= 1 &&
            bc != strong_side) {
        scale[0] = scale[1] = 0;
    }
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

static const int edge_score[8] = { 10, 8, 4, 1, 1, 4, 8, 10 };
 
static int score_win(const position_t* pos, eval_data_t* ed)
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[weak_side] == 1);
    assert(pos->num_pawns[weak_side] == 0);

    square_t wk = pos->pieces[strong_side][0];
    square_t bk = pos->pieces[weak_side][0];
    int cornered = edge_score[square_rank(bk)] * edge_score[square_file(bk)];
    int score = pos->material_eval[strong_side] - KING_VAL +
        cornered - distance(wk, bk);
    if (pos->num_pieces[create_piece(strong_side, QUEEN)] +
            pos->num_pieces[create_piece(strong_side, ROOK)]) {
        score += WON_ENDGAME;
    }
    return score * (strong_side == pos->side_to_move ? 1 : -1);
}

static int score_draw(const position_t* pos, eval_data_t* ed)
{
    (void)pos; (void)ed;
    return DRAW_VALUE;
}

static int score_kbnk(const position_t* pos, eval_data_t* ed)
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 3);
    assert(pos->num_pawns[strong_side] == 0);
    assert(pos->num_pieces[weak_side] == 1);
    assert(pos->num_pawns[weak_side] == 0);

    square_t wk = pos->pieces[strong_side][0];
    square_t wb = pos->pieces[strong_side][1];
    square_t bk = pos->pieces[weak_side][0];
    assert(piece_type(pos->board[wb]) == BISHOP);
    color_t bc = square_color(wb);
    square_t t1 = bc == WHITE ? A8 : A1;
    square_t t2 = bc == WHITE ? H1 : H8;
    int corner_dist = MIN(distance(bk, t1), distance(bk, t2)) +
        MIN(square_rank(bk), square_file(bk));
    return (WON_ENDGAME - 10*corner_dist - distance(wk, bk)) *
        (strong_side == pos->side_to_move ? 1 : -1);
}

static int score_krpkr(const position_t* pos, eval_data_t* ed)
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[weak_side] == 2);
    assert(pos->num_pawns[weak_side] == 0);

    square_t wk = pos->pieces[strong_side][0];
    square_t bk = pos->pieces[weak_side][0];
    square_t wr = pos->pieces[strong_side][1];
    square_t br = pos->pieces[weak_side][1];
    square_t wp = pos->pawns[strong_side][0];
    bool tempo = strong_side == pos->side_to_move;
 
    if (strong_side == BLACK) {
        wk = mirror_rank(wk);
        bk = mirror_rank(bk);
        wr = mirror_rank(wr);
        br = mirror_rank(br);
        wp = mirror_rank(wp);
    }
    if (square_file(wp) >= FILE_E) {
        wk = mirror_file(wk);
        bk = mirror_file(bk);
        wr = mirror_file(wr);
        br = mirror_file(br);
        wp = mirror_file(wp);
    }

    int wk_rank = square_rank(wk);
    int br_rank = square_rank(br);
    int wr_rank = square_rank(wr);
    int p_rank = square_rank(wp);
    int p_file = square_file(wp);
    int prom_sq = p_file + A8;

    // Lots of tricky rook endgames. Credit goes to Silman's Complete Endgame
    // Course (great book), checked against Stockfish's implementation.
    // I'm horrible at rook endgames over the board, so yeah.

    // The Philidor position is a draw.
    if (p_rank < RANK_6 && distance(bk, prom_sq) <= 1 && wk_rank < RANK_6 &&
            (((p_rank < RANK_4 && wr_rank != RANK_6)) ||
            br_rank == RANK_6)) return 0;

    // Philidor defense after white pushes the pawn. Black needs to check
    // the white king from behind.
    int br_file = square_file(br);
    if (p_rank >= RANK_6 && distance(bk, prom_sq) <= 1 &&
            wk_rank + tempo <= RANK_6 &&
            (br_rank == RANK_1 || (abs(br_file - p_file) > 2))) return 0;

    if (p_rank >= RANK_6 && bk == prom_sq && br_rank == RANK_1 &&
            (!tempo || distance(wk, wp) > 1)) return 0;

    // Fiddly rook-pawn promotion blocked by friendly rook. G7 and H7 are
    // the only places the black king is safe.
    int wk_file = square_file(wk);
    if (wp == A7 && wr == A8 && (bk == G7 || bk == H7) &&
            (br_file == FILE_A && (br_rank < RANK_4 ||
                                   wk_file > FILE_C ||
                                   wk_rank < RANK_6))) return 0;

    // Blockaded pawn with the white king far away is a draw.
    if (p_rank < RANK_6 && bk == wp + N &&
            distance(wk, wp) - tempo > 1 &&
            distance(wk, br) - tempo > 1) return 0;

    // Generic supported pawn on 7 usually wins if the king is close.
    int wr_file = square_file(wr);
    int val = 0;
    if (p_rank == RANK_7 && p_file != FILE_A &&
            wr_file == p_file && wr != prom_sq &&
            distance(wk, prom_sq) < distance(bk, prom_sq) - 2 + tempo &&
            distance(wk, prom_sq) < distance(bk, wr + tempo)) {
        val = EG_ROOK_VAL - distance(wp, prom_sq) + tempo;
    }

    // Generic less-advanced pawn, less favorable.
    if (!val && p_file != FILE_A && wr_file == p_file && wr < wp &&
            distance(wk, prom_sq) < distance(bk, prom_sq) - 2 + tempo &&
            distance(wk, wp + N) < distance(bk, wp + N) - 2 + tempo &&
            (distance(bk, wr) + tempo > 2 ||
             (distance(wk, prom_sq) < distance(bk, wr) + tempo &&
              distance(wk, wp + N) < distance(bk, wr) + tempo))) {
        val = EG_PAWN_VAL - distance(wk, prom_sq) + 16*p_rank;
    }

    // Blocked, unsupported pawn; usually a draw.
    int bk_file = square_file(bk);
    if (!val && p_rank < RANK_5 && bk > wp) {
        if (bk_file == p_file) val =  5;
        else if (abs(bk_file - p_file) == 1 && distance(wk, bk) > 2) val = 10;
    }
    if (!val) val = EG_PAWN_VAL + p_rank*5 - (p_file == FILE_A ? 25 : 0);
    return val * (strong_side == pos->side_to_move ? 1 : -1);
}
