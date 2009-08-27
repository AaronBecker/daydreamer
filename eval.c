
#include "daydreamer.h"

#ifdef UFO_EVAL
#include "pst_ufo.inc"
#else
#include "pst.inc"
#endif

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
}

/*
 * Perform a simple position evaluation based just on material and piece
 * square bonuses.
 */
int simple_eval(const position_t* pos)
{
    color_t side = pos->side_to_move;
    int material_eval = pos->material_eval[side] - pos->material_eval[side^1];
    float phase = game_phase(pos);
    int piece_square_eval = (int)(
            (phase)*(pos->piece_square_eval[side] -
                pos->piece_square_eval[side^1]) +
            (1-phase)*(pos->endgame_piece_square_eval[side] -
                pos->endgame_piece_square_eval[side^1]));
    int material_adjust = 0;
#ifndef UFO_EVAL
    // Adjust material based on Larry Kaufmans's formula in
    // "The Evaluation of Material Imbalances"
    // bishop pair: +50
    // knight += 5 * (pawns-5)
    // rook -= 10 * (pawns-5)
    material_adjust += pos->piece_count[WB] > 1 ? 50 : 0;
    material_adjust -= pos->piece_count[BB] > 1 ? 50 : 0;
    material_adjust += pos->piece_count[WN] * 5 * (pos->piece_count[WP] - 5);
    material_adjust -= pos->piece_count[BN] * 5 * (pos->piece_count[BP] - 5);
    material_adjust -= pos->piece_count[WR] * 10 * (pos->piece_count[WP] - 5);
    material_adjust += pos->piece_count[BR] * 10 * (pos->piece_count[BP] - 5);
    if (side == BLACK) material_adjust *= -1;
#endif
    return material_eval + piece_square_eval + material_adjust;
}

/*
 * Do full, more expensive evaluation of the position. Not implemented yet,
 * so just return the simple evaluation.
 */
int full_eval(const position_t* pos)
{
    int score = simple_eval(pos);
#ifndef UFO_EVAL
    score_t phase_score;
    phase_score.endgame = phase_score.midgame = 0;
    pawn_score(pos, &phase_score);
    // It's not worth computing mobility if we're way ahead or behind.
    if (abs(score) < 2.5 * PAWN_VAL) mobility_score(pos, &phase_score);
    float phase = game_phase(pos);
    score += phase*phase_score.midgame + (1-phase)*phase_score.endgame;
#endif
    if (!can_win(pos, pos->side_to_move)) score = MIN(score, DRAW_VALUE);
    if (!can_win(pos, pos->side_to_move^1)) score = MAX(score, DRAW_VALUE);
    return score;
}

void report_eval(const position_t* pos)
{
    color_t side = pos->side_to_move;
    int material_eval = pos->material_eval[side] - pos->material_eval[side^1];
    float phase = game_phase(pos);
    printf("info string game phase: %.4f\n", phase);
    printf("info string material: %d\n", material_eval);
    printf("info string psq midgame: %d - %d = %d\n",
            pos->piece_square_eval[side],
            pos->piece_square_eval[side^1],
            pos->piece_square_eval[side] - pos->piece_square_eval[side^1]);
    printf("info string psq endgame: %d - %d = %d\n",
            pos->endgame_piece_square_eval[side],
            pos->endgame_piece_square_eval[side^1],
            pos->endgame_piece_square_eval[side] -
            pos->endgame_piece_square_eval[side^1]);
    int piece_square_eval = (int)(
            (phase)*(pos->piece_square_eval[side] -
                pos->piece_square_eval[side^1]) +
            (1-phase)*(pos->endgame_piece_square_eval[side] -
                pos->endgame_piece_square_eval[side^1]));
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

    score_t phase_score;
    phase_score.endgame = phase_score.midgame = 0;
    pawn_score(pos, &phase_score);
    printf("info string pawns (mid,end): (%d, %d)\n",
            phase_score.midgame, phase_score.endgame); 
    // It's not worth computing mobility if we're way ahead or behind.
    score_t mob_score;
    mob_score.endgame = mob_score.midgame = 0;
    mobility_score(pos, &mob_score);
    printf("info string mob (mid,end): (%d, %d)\n",
            mob_score.midgame, mob_score.endgame); 
    phase_score.midgame += mob_score.midgame;
    phase_score.endgame += mob_score.endgame;
    int score = material_eval + piece_square_eval + material_adjust +
        phase*phase_score.midgame + (1-phase)*phase_score.endgame;
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
 * Is the given position an endgame? Use a rough heuristic for now; it's used
 * to figure out which piece square tables to consult.
 */
float game_phase(const position_t* pos)
{
    // TODO: game phase determination could be a lot more sophisticated,
    // and the information could be cached.
    static const float total_possible_material =
        16*PAWN_VAL + 4*KNIGHT_VAL + 4*BISHOP_VAL + 4*ROOK_VAL + 2*QUEEN_VAL;
    float phase = (pos->material_eval[WHITE]+pos->material_eval[BLACK] -
            2*KING_VAL) / total_possible_material;
    return MIN(phase, 1.0);
}
