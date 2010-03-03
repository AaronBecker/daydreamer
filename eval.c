
#include "daydreamer.h"

#include "pst.inc"

static const int pawn_scale = 1024;
static const int pattern_scale = 1024;
static const int pieces_scale = 1024;
static const int shield_scale = 1024+128;
static const int king_attack_scale = 1024;

/*
 * Initialize all static evaluation data structures.
 */
void init_eval(void)
{
    for (piece_t piece=WP; piece<=WK; ++piece) {
        for (square_t square=A1; square<=H8; ++square) {
            if (!valid_board_index(square)) continue;
            piece_square_values[piece][square] =
                piece_square_values[piece+BP-1][flip_square(square)];
            endgame_piece_square_values[piece][square] =
                endgame_piece_square_values[piece+BP-1][flip_square(square)];
        }
    }
    for (piece_t piece=WP; piece<=BK; ++piece) {
        if (piece > WK && piece < BP) continue;
        for (square_t square=A1; square<=H8; ++square) {
            if (!valid_board_index(square)) continue;
            piece_square_values[piece][square] += material_value(piece);
            endgame_piece_square_values[piece][square] +=
                eg_material_value(piece);
        }
    }
}

/*
 * Combine two scores, scaling |addend| by the given factor.
 */
static void add_scaled_score(score_t* score, score_t* addend, int scale)
{
    score->midgame += addend->midgame * scale / 1024;
    score->endgame += addend->endgame * scale / 1024;
}

/*
 * Blend endgame and midgame values linearly according to |phase|.
 */
static int blend_score(score_t* score, int phase)
{
    return (phase*score->midgame + (MAX_PHASE-phase)*score->endgame)/MAX_PHASE;
}

/*
 * Perform a simple position evaluation based just on material and piece
 * square bonuses.
 */
int simple_eval(const position_t* pos)
{
    color_t side = pos->side_to_move;
    eval_data_t ed;
    ed.md = get_material_data(pos);

    int score = 0;
    int endgame_scale[2];
    if (scale_endgame(pos, &ed, endgame_scale, &score)) return score;

    score_t phase_score = ed.md->score;
    if (side == BLACK) {
        phase_score.midgame *= -1;
        phase_score.endgame *= -1;
    }
    phase_score.midgame += pos->piece_square_eval[side].midgame -
        pos->piece_square_eval[side^1].midgame;
    phase_score.endgame += pos->piece_square_eval[side].endgame -
        pos->piece_square_eval[side^1].endgame;

    // Tempo
    phase_score.midgame += 9;
    phase_score.endgame += 2;

    score = blend_score(&phase_score, ed.md->phase);
    score = (score * endgame_scale[score > 0 ? side : side^1]) / 16;

    if (!can_win(pos, side)) score = MIN(score, DRAW_VALUE);
    if (!can_win(pos, side^1)) score = MAX(score, DRAW_VALUE);
    return score;
}

/*
 * Do full, more expensive evaluation of the position. Not implemented yet,
 * so just return the simple evaluation.
 */
int full_eval(const position_t* pos, eval_data_t* ed)
{
    color_t side = pos->side_to_move;
    score_t phase_score, component_score;
    component_score = pawn_score(pos, &ed->pd);
    ed->md = get_material_data(pos);

    int score = 0;
    int endgame_scale[2];
    if (scale_endgame(pos, ed, endgame_scale, &score)) return score;

    phase_score = ed->md->score;
    if (side == BLACK) {
        phase_score.midgame *= -1;
        phase_score.endgame *= -1;
    }
    phase_score.midgame += pos->piece_square_eval[side].midgame -
        pos->piece_square_eval[side^1].midgame;
    phase_score.endgame += pos->piece_square_eval[side].endgame -
        pos->piece_square_eval[side^1].endgame;

    add_scaled_score(&phase_score, &component_score, pawn_scale);
    component_score = pattern_score(pos);
    add_scaled_score(&phase_score, &component_score, pattern_scale);
    component_score = pieces_score(pos, ed->pd);
    add_scaled_score(&phase_score, &component_score, pieces_scale);
    component_score = evaluate_king_shield(pos);
    add_scaled_score(&phase_score, &component_score, shield_scale);
    component_score = evaluate_king_attackers(pos);
    add_scaled_score(&phase_score, &component_score, king_attack_scale);

    // Tempo
    phase_score.midgame += 9;
    phase_score.endgame += 2;

    score = blend_score(&phase_score, ed->md->phase);
    score = (score * endgame_scale[score > 0 ? side : side^1]) / 16;

    if (!can_win(pos, side)) score = MIN(score, DRAW_VALUE);
    if (!can_win(pos, side^1)) score = MAX(score, DRAW_VALUE);
    return score;
}

/*
 * Print a breakdown of the static evaluation of |pos|.
 */
void report_eval(const position_t* pos)
{
    color_t side = pos->side_to_move;
    int material_eval = pos->material_eval[side] - pos->material_eval[side^1];
    int phase = game_phase(pos);
    printf("info string game phase: %d\n", phase);
    printf("info string material: %d\n", material_eval);
    printf("info string psq midgame: %d - %d = %d\n",
            pos->piece_square_eval[side].midgame,
            pos->piece_square_eval[side^1].midgame,
            pos->piece_square_eval[side].midgame -
            pos->piece_square_eval[side^1].midgame);
    printf("info string psq endgame: %d - %d = %d\n",
            pos->piece_square_eval[side].endgame,
            pos->piece_square_eval[side^1].endgame,
            pos->piece_square_eval[side].endgame -
            pos->piece_square_eval[side^1].endgame);
    int piece_square_eval =
            ((phase)*(pos->piece_square_eval[side].midgame -
                pos->piece_square_eval[side^1].midgame) +
            (MAX_PHASE-phase)*(pos->piece_square_eval[side].endgame -
                pos->piece_square_eval[side^1].endgame)) / MAX_PHASE;
    printf("info string psq score: %d\n", piece_square_eval);

    int material_adjust = 0;
    material_adjust += pos->piece_count[WB] > 1 ? 50 : 0;
    material_adjust -= pos->piece_count[BB] > 1 ? 50 : 0;
    printf("info string mat_adj bishop (w,b): (%d, %d)\n", 
            pos->piece_count[WB] > 1 ? 50 : 0,
            pos->piece_count[BB] > 1 ? 50 : 0);
    material_adjust += pos->piece_count[WN] * 5 * (pos->piece_count[WP] - 5);
    material_adjust -= pos->piece_count[BN] * 5 * (pos->piece_count[BP] - 5);
    printf("info string mat_adj knight: (%d, %d)\n",
            pos->piece_count[WN] * 5 * (pos->piece_count[WP] - 5),
            pos->piece_count[BN] * 5 * (pos->piece_count[BP] - 5));
    material_adjust -= pos->piece_count[WR] * 10 * (pos->piece_count[WP] - 5);
    material_adjust += pos->piece_count[BR] * 10 * (pos->piece_count[BP] - 5);
    printf("info string mat_adj rook: (%d, %d)\n",
            -pos->piece_count[WR] * 10 * (pos->piece_count[WP] - 5),
            -pos->piece_count[BR] * 10 * (pos->piece_count[BP] - 5));
    if (side == BLACK) material_adjust *= -1;
    printf("info string mat_adj total: %d\n", material_adjust);

    score_t king_score;
    king_score.endgame = king_score.midgame = 0;
    king_score = evaluate_king_attackers(pos);
    printf("info string king attackers: %d\n", king_score.midgame);
    king_score = evaluate_king_shield(pos);
    printf("info string king shield: %d\n", king_score.midgame);

    score_t phase_score;
    phase_score.endgame = phase_score.midgame = 0;
    pawn_data_t* pd;
    score_t p_score = pawn_score(pos, &pd);
    printf("info string pawns (mid,end): (%d, %d)\n",
            p_score.midgame, p_score.endgame);
    add_scaled_score(&phase_score, &p_score, 1024);
    score_t pc_score = pieces_score(pos, pd);
    printf("info string pieces (mid,end): (%d, %d)\n",
            pc_score.midgame, pc_score.endgame);
    add_scaled_score(&phase_score, &pc_score, 1024);
    int score = material_eval + piece_square_eval + material_adjust +
        blend_score(&phase_score, phase);
    printf("info string score: %d\n", score);
}

/*
 * Is there enough material left for either side to conceivably win?
 */
bool insufficient_material(const position_t* pos)
{
    return (pos->num_pawns[WHITE] == 0 &&
        pos->num_pawns[BLACK] == 0 &&
        pos->material_eval[WHITE] < ROOK_VAL + KING_VAL &&
        pos->material_eval[BLACK] < ROOK_VAL + KING_VAL);
}

/*
 * Is it still possible for |side| to win the game?
 */
bool can_win(const position_t* pos, color_t side)
{
    return !(pos->num_pawns[side] == 0 &&
            pos->material_eval[side] < ROOK_VAL + KING_VAL);
}

/*
 * Is the given position definitely a draw. This considers only draws by
 * rule, not positions that are known to be drawable with best play.
 */
bool is_draw(const position_t* pos)
{
    return pos->fifty_move_counter > 100 ||
        (pos->fifty_move_counter == 100 && !is_check(pos)) ||
        insufficient_material(pos) ||
        is_repetition(pos);
}


