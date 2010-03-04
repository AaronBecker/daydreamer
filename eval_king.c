
#include "daydreamer.h"

static const int shield_scale = 1024+128;
static const int king_attack_scale = 1024;

const int shield_value[2][17] = {
    { 0, 8, 2, 4, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 2, 4, 1, 1, 0, 0, 0 },
};

const int king_attack_score[16] = {
    0, 5, 20, 20, 40, 80, 0, 0, 0, 5, 20, 20, 40, 80, 0, 0
};
const int multiple_king_attack_scale[16] = {
    0, 0, 512, 640, 896, 960, 1024, 1024,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024
};

//static void evaluate_king_shield(const position_t* pos, int score[2]);
static int king_shield_score(const position_t* pos, color_t score);
//static int evaluate_king_attackers(const position_t* pos, int shield_score[2]);

score_t king_safety_score(const position_t* pos, eval_data_t* ed)
{
    (void)ed;
    int score[2] = {0, 0};
    int shield_score[2] = {0, 0};
    for (color_t side = WHITE; side <= BLACK; ++side) {
        shield_score[side] = king_shield_score(pos, side);
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
            multiple_king_attack_scale[num_attackers] * king_attack_scale /
            (1024*1024);
        //score[side] = (score[side]*
        //        (200 - MIN(100, shield_score[side]))/75)*
        //    multiple_king_attack_scale[num_attackers] / 1024 *
        //    king_attack_scale / 1024;
    }
    color_t side = pos->side_to_move;
    score_t phase_score;
    phase_score.endgame = 0;
    phase_score.midgame = score[side] - score[side^1] +
        shield_score[side] - shield_score[side^1];
    return phase_score;
}

/*
 * Give a bonus for blocking pieces around a square.
 */
static int shield_score(const position_t* pos, color_t side, square_t king)
{
    int s = 0;
    int push = pawn_push[side];
    s += shield_value[side][pos->board[king-1]] * 2;
    s += shield_value[side][pos->board[king+1]] * 2;
    s += shield_value[side][pos->board[king+push-1]] * 4;
    s += shield_value[side][pos->board[king+push]] * 6;
    s += shield_value[side][pos->board[king+push+1]] * 4;
    s += shield_value[side][pos->board[king+2*push-1]];
    s += shield_value[side][pos->board[king+2*push]] * 2;
    s += shield_value[side][pos->board[king+2*push+1]];
    return s;
}

/*
 * Compute the overall balance of king safety offered by pawn shields.
 */
static int king_shield_score(const position_t* pos, color_t side)
{
    //if (pos->piece_count[BQ] + pos->piece_count[WQ] == 0) return phase_score;
    int score, oo_score = 0, ooo_score = 0, castle_score;

    score = shield_score(pos, side, pos->pieces[side][0]);
    if (has_oo_rights(pos, side)) {
        oo_score = shield_score(pos, side, G1 + A8*side);
    }
    if (has_ooo_rights(pos, side)) {
        ooo_score = shield_score(pos, side, C1 + A8*side);
    }
    castle_score = MAX(score, MAX(oo_score, ooo_score));
    return (score + castle_score) * shield_scale / 2048;
}

/*
 * Compute the overall balance of king safety offered by pawn shields.
 */
//static void evaluate_king_shield(const position_t* pos, int score[2])
//{
//    //if (pos->piece_count[BQ] + pos->piece_count[WQ] == 0) return phase_score;
//    int oo_score[2] = {0, 0};
//    int ooo_score[2] = {0, 0};
//    int castle_score[2] = {0, 0};
//
//    score[WHITE] = king_shield_score(pos, WHITE, pos->pieces[WHITE][0]);
//    if (has_oo_rights(pos, WHITE)) {
//        oo_score[WHITE] = king_shield_score(pos, WHITE, G1);
//    }
//    if (has_ooo_rights(pos, WHITE)) {
//        ooo_score[WHITE] = king_shield_score(pos, WHITE, C1);
//    }
//    castle_score[WHITE] = MAX(score[WHITE],
//        MAX(oo_score[WHITE], ooo_score[WHITE]));
//
//    score[BLACK] = king_shield_score(pos, BLACK, pos->pieces[BLACK][0]);
//    if (has_oo_rights(pos, BLACK)) {
//        oo_score[BLACK] = king_shield_score(pos, BLACK, G8);
//    }
//    if (has_ooo_rights(pos, WHITE)) {
//        ooo_score[BLACK] = king_shield_score(pos, BLACK, C8);
//    }
//    castle_score[BLACK] = MAX(score[BLACK],
//        MAX(oo_score[BLACK], ooo_score[BLACK]));
//
//    score[WHITE] = (score[WHITE] + castle_score[WHITE]) * shield_scale / 2048;
//    score[BLACK] = (score[BLACK] + castle_score[BLACK]) * shield_scale / 2048;
//}

/*
 * Compute a measure of king safety given by the number and type of pieces
 * attacking a square adjacent to the king.
 * TODO: This could probably be a lot more sophisticated.
 */
//static int evaluate_king_attackers(const position_t* pos, int shield_score[2])
//{
//    int score[2] = {0, 0};
//    for (color_t side = WHITE; side <= BLACK; ++side) {
//        score[side] = shield_score[side];
//        if (pos->piece_count[create_piece(side, QUEEN)] == 0) continue;
//        const square_t opp_king = pos->pieces[side^1][0];
//        int num_attackers = 0;
//        for (int i=1; i<pos->num_pieces[side]; ++i) {
//            const square_t attacker = pos->pieces[side][i];
//            if (piece_attacks_near(pos, attacker, opp_king)) {
//                score[side] += king_attack_score[pos->board[attacker]];
//                num_attackers++;
//            }
//        }
//        score[side] += score[side] *
//            multiple_king_attack_scale[num_attackers] / 1024 *
//            king_attack_scale / 1024;
//        //score[side] = (score[side]*
//        //        (200 - MIN(100, shield_score[side]))/75)*
//        //    multiple_king_attack_scale[num_attackers] / 1024 *
//        //    king_attack_scale / 1024;
//    }
//    color_t side = pos->side_to_move;
//    return score[side]-score[side^1];
//    //return ((score[side]-score[side^1])*king_attack_scale)/1024 +
//    //    ((shield_score[side]-shield_score[side^1])*shield_scale)/1024;
//}
