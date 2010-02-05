
#include "daydreamer.h"

#ifdef UFO_EVAL
#include "pst_ufo.inc"
#else
#include "pst.inc"
#endif

static const int pawn_scale = 768;
// values tested: 640, (768), 896, 1024, 1536
static const int pattern_scale = 1024;
// values tested: 768, (1024), 1280
static const int pieces_scale = 1024;
// values tested: 768, (1024), 1280
static const int shield_scale = 1704;
// values tested: 1024, 1280, 1576, (1704), 2048
static const int king_attack_scale = 1024;
// values tested: 640, 768, (896), 1024, 1536

const int shield_value[2][17] = {
    { 0, 4, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 1, 2, 0, 0, 0, 0, 0 },
};

const int king_attack_score[16] = {
    0, 5, 20, 20, 40, 80, 0, 0, 0, 5, 20, 20, 40, 80, 0, 0
};
const int multiple_king_attack_scale[16] = {
    0, 128, 512, 640, 896, 960, 1024, 1024,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024
};

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
    return (phase*score->midgame + (1024-phase)*score->endgame) / 1024;
}

/*
 * Give some points for pawns directly in front of your king.
 */
static int king_shield_score(const position_t* pos, color_t side, square_t king)
{
    int s = 0;
    int push = pawn_push[side];
    s += shield_value[side][pos->board[king-1]] / 2;
    s += shield_value[side][pos->board[king+1]] / 2;
    s += shield_value[side][pos->board[king+push-1]];
    s += shield_value[side][pos->board[king+push]] * 2;
    s += shield_value[side][pos->board[king+push+1]];
    s += shield_value[side][pos->board[king+2*push-1]] / 2;
    s += shield_value[side][pos->board[king+2*push]];
    s += shield_value[side][pos->board[king+2*push+1]] / 2;
    return s;
}

/*
 * Compute the overall balance of king safety offered by pawn shields.
 */
static score_t evaluate_king_shield(const position_t* pos)
{
    int score[2] = {0, 0};
    if (pos->piece_count[WQ] != 0) {
        score[WHITE] = king_shield_score(pos, WHITE, pos->pieces[WHITE][0]);
    }
    if (pos->piece_count[BQ] != 0) {
        score[BLACK] = king_shield_score(pos, BLACK, pos->pieces[BLACK][0]);
    }
    color_t side = pos->side_to_move;
    score_t phase_score;
    phase_score.midgame = score[side]-score[side^1];
    phase_score.endgame = 0;
    return phase_score;
}

/*
 * Compute a measure of king safety given by the number and type of pieces
 * attacking a square adjacent to the king.
 * TODO: This could probably be a lot more sophisticated.
 */
static score_t evaluate_king_attackers(const position_t* pos)
{
    int score[2] = {0, 0};
    for (color_t side = WHITE; side <= BLACK; ++side) {
        if (pos->piece_count[create_piece(side, QUEEN)] == 0) continue;
        const square_t opp_king = pos->pieces[side^1][0];
        int num_attackers = 0;
        for (int i=1; i<pos->num_pieces[side]; ++i) {
            const square_t attacker = pos->pieces[side][i];
            if (piece_attacks_near(pos, attacker, opp_king)) {
                score[side] += king_attack_score[pos->board[attacker]];
                num_attackers++;
            }
        }
        score[side] = score[side] *
            multiple_king_attack_scale[num_attackers] / 1024;
    }
    color_t side = pos->side_to_move;
    score_t phase_score;
    phase_score.midgame = score[side]-score[side^1];
    phase_score.endgame = 0;
    return phase_score;
}

/*
 * Perform a simple position evaluation based just on material and piece
 * square bonuses.
 */
int simple_eval(const position_t* pos)
{
    color_t side = pos->side_to_move;
    int phase = game_phase(pos);
    score_t phase_score;
    // Material + PST
    phase_score.midgame = pos->piece_square_eval[side].midgame -
        pos->piece_square_eval[side^1].midgame;
    phase_score.endgame = pos->piece_square_eval[side].endgame -
        pos->piece_square_eval[side^1].endgame;
    return blend_score(&phase_score, phase);
}

/*
 * Do full, more expensive evaluation of the position. Not implemented yet,
 * so just return the simple evaluation.
 */
int full_eval(const position_t* pos, eval_data_t* ed)
{
#ifdef UFO_EVAL
    return simple_eval(pos);
#endif
    color_t side = pos->side_to_move;
    score_t phase_score, component_score;
    component_score = pawn_score(pos, &ed->pd);
    ed->md = get_material_data(pos);

    int score = 0;
    //if (endgame_score(pos, ed, &score)) return score;

    int endgame_scale[2];
    determine_endgame_scale(pos, ed, endgame_scale);
    if (endgame_scale[WHITE]==0 && endgame_scale[BLACK]==0) return DRAW_VALUE;

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
            (1024-phase)*(pos->piece_square_eval[side].endgame -
                pos->piece_square_eval[side^1].endgame)) / 1024;
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
    return pos->fifty_move_counter >= 100 ||
        insufficient_material(pos) ||
        is_repetition(pos);
}

/*
 * Is this position an opening or an endgame? Scored on a scale of 0-1024,
 * with 1024 being a pure opening and 0 a pure endgame.
 */
int game_phase(const position_t* pos)
{
    static const int max_scaled_count = 4 + 4 + 8 + 8;
    int scaled_count = pos->piece_count[WN] + pos->piece_count[WB] +
        2*pos->piece_count[WR] + 4*pos->piece_count[WQ] +
        pos->piece_count[BN] + pos->piece_count[BB] +
        2*pos->piece_count[BR] + 4*pos->piece_count[BQ];
    int phase = scaled_count * 1024 / max_scaled_count;
    return CLAMP(phase, 0, 1024);
}
