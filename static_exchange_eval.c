
#include "daydreamer.h"

/*
 * Count all attackers and defenders of a square to determine whether or not
 * a capture is advantageous. Captures with a positive static eval are
 * favorable. Note: this implementation does not account for pinned pieces.
 */
int static_exchange_eval(position_t* pos, move_t move)
{
    square_t attacker_sq = get_move_from(move);
    square_t attacked_sq = get_move_to(move);
    piece_t attacker = get_move_piece(move);
    piece_t captured = get_move_capture(move);
    if (!captured) return 0; // no capture--exchange is even
    square_t attacker_sqs[2][16];
    piece_t attacker_pcs[2][16];
    int num_attackers[2] = { 0, 0 };
    int initial_attacker[2] = { 0, 0 };

    int index;
    square_t sq;
    // Find all the non-sliding pieces that could be attacking.
    // Pawns:
    if (pos->board[attacked_sq + NW] == BP && attacked_sq + NW != attacker_sq) {
        index = num_attackers[BLACK]++;
        attacker_sqs[BLACK][index] = attacked_sq + NW;
        attacker_pcs[BLACK][index] = BP;
    }
    if (pos->board[attacked_sq + NE] == BP && attacked_sq + NE != attacker_sq) {
        index = num_attackers[BLACK]++;
        attacker_sqs[BLACK][index] = attacked_sq + NE;
        attacker_pcs[BLACK][index] = BP;
    }
    if (pos->board[attacked_sq + SW] == WP && attacked_sq + SW != attacker_sq) {
        index = num_attackers[WHITE]++;
        attacker_sqs[WHITE][index] = attacked_sq + SW;
        attacker_pcs[WHITE][index] = WP;
    }
    if (pos->board[attacked_sq + SE] == WP && attacked_sq + SE != attacker_sq) {
        index = num_attackers[WHITE]++;
        attacker_sqs[WHITE][index] = attacked_sq + SE;
        attacker_pcs[WHITE][index] = WP;
    }
    // Knights:
    for (int i=0; i<pos->piece_count[WHITE][KNIGHT]; ++i) {
        sq = pos->pieces[WHITE][KNIGHT][i];
        if (sq != attacker_sq && possible_attack(sq, attacked_sq, WN)) {
            index = num_attackers[WHITE]++;
            attacker_sqs[WHITE][index] = sq;
            attacker_pcs[WHITE][index] = WN;
        }
    }
    for (int i=0; i<pos->piece_count[BLACK][KNIGHT]; ++i) {
        sq = pos->pieces[BLACK][KNIGHT][i];
        if (sq != attacker_sq && possible_attack(sq, attacked_sq, BN)) {
            index = num_attackers[BLACK]++;
            attacker_sqs[BLACK][index] = sq;
            attacker_pcs[BLACK][index] = BN;
        }
    }
    // Kings:
    sq = pos->pieces[WHITE][KING][0];
    if (sq != attacker_sq && possible_attack(sq, attacked_sq, WK)) {
        index = num_attackers[WHITE]++;
        attacker_sqs[WHITE][index] = sq;
        attacker_pcs[WHITE][index] = WK;
    }
    sq = pos->pieces[BLACK][KING][0];
    if (sq != attacker_sq && possible_attack(sq, attacked_sq, BK)) {
        index = num_attackers[BLACK]++;
        attacker_sqs[BLACK][index] = sq;
        attacker_pcs[BLACK][index] = BK;
    }

    for (int side=WHITE; side<=BLACK; ++side) {
        for (int type=BISHOP; type<=QUEEN; ++type) {
            //const int num_pieces = pos->piece_count[side][type];
            const piece_t p = create_piece(side, type);
            //for (int i=0; i<num_pieces; ++i) {
            square_t from;
            for (square_t* pfrom = &pos->pieces[side][type][0];
                    (from = *pfrom) != INVALID_SQUARE;
                    ++pfrom) {
                //const square_t from = pos->pieces[side][type][i];
                if (from == attacker_sq || 
                        !possible_attack(from, attacked_sq, p)) continue;
                square_t att_sq = from;
                direction_t att_dir = direction(from, attacked_sq);
                while (true) {
                    att_sq += att_dir;
                    if (att_sq == attacked_sq) {
                        index = num_attackers[side]++;
                        attacker_sqs[side][index] = from;
                        attacker_pcs[side][index] = p;
                        break;
                    }
                    if (pos->board[att_sq]) break;
                }
            }
        }
    }
    // At this point, all unblocked attackers other than |attacker| have been
    // added to |attackers|. Now play out all possible captures in order of
    // increasing piece value (but starting with |attacker|) while alternating
    // colors. Whenever a capture is made, add any x-ray attackers that removal
    // of the piece would reveal.
    color_t side = piece_color(attacker);
    initial_attacker[side] = 1;
    int gain[32] = { material_value(captured) };
    int gain_index = 1;
    int capture_value = material_value(attacker);
    bool initial_attack = true;
    while (num_attackers[side] + initial_attacker[side] + 1 || initial_attack) {
        initial_attack = false;
        // add in any new x-ray attacks
        const attack_data_t* att_data =
            &get_attack_data(attacker_sq, attacked_sq);
        if (att_data->possible_attackers & Q_FLAG) {
            square_t sq;
            for (sq=attacker_sq - att_data->relative_direction;
                    valid_board_index(sq) && pos->board[sq] != EMPTY;
                    sq -= att_data->relative_direction) {}
            if (valid_board_index(sq) && pos->board[sq] != EMPTY) {
                const piece_t xray_piece = pos->board[sq];
                if (att_data->possible_attackers & 
                        get_piece_flag(xray_piece)) {
                    const color_t xray_color = piece_color(xray_piece);
                    index = num_attackers[xray_color]++;
                    attacker_sqs[xray_color][index] = sq;
                    attacker_pcs[xray_color][index] = xray_piece;
                }
            }
        }

        // score the capture under the assumption that it's defended.
        gain[gain_index] = capture_value - gain[gain_index - 1];
        ++gain_index;
        side ^= 1;

        // find the next lowest valued attacker
        int least_value = material_value(KING)+1;
        int att_index = -1;
        for (int i=0; i<num_attackers[side]; ++i) {
            capture_value = material_value(attacker_pcs[side][i]);
            if (capture_value < least_value) {
                least_value = capture_value;
                att_index = i;
            }
        }
        if (att_index == -1) {
            assert(num_attackers[side] == 0);
            break;
        }
        attacker = attacker_pcs[side][att_index];
        attacker_sq = attacker_sqs[side][att_index];
        index = --num_attackers[side];
        attacker_sqs[side][att_index] = attacker_sqs[side][index];
        attacker_pcs[side][att_index] = attacker_pcs[side][index];
        capture_value = least_value;
        if (piece_type(attacker) == KING && num_attackers[side^1]) break;
    }

    // Now that gain array is set up, scan back through to get score.
    --gain_index;
    while (--gain_index) {
        gain[gain_index-1] = -gain[gain_index-1] < gain[gain_index] ?
            -gain[gain_index] : gain[gain_index-1];
    }
    return gain[0];
}
