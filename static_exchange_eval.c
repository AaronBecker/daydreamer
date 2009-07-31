
#include "daydreamer.h"

/*
 * Count all attackers and defenders of a square to determine whether or not
 * a capture is advantageous. Captures with a positvie static eval are
 * favorable. Note: this implementation does not account for pinned pieces.
 */
int static_exchange_eval(position_t* pos, move_t move)
{
    square_t attacker_sq = get_move_from(move);
    square_t attacked_sq = get_move_to(move);
    piece_t attacker = get_move_piece(move);
    piece_t captured = get_move_capture(move);
    if (!captured) return 0; // no capture--exchange is even
    const piece_entry_t* attackers[2][16];
    int num_attackers[2] = { 0, 0 };
    int initial_attacker[2] = { 0, 0 };

    for (color_t side=WHITE; side<=BLACK; ++side) {
        for (piece_t p=PAWN; p<=KING; ++p) {
            const int num_pieces = pos->piece_count[side][p];
            for (int i=0; i<num_pieces; ++i) {
                const piece_entry_t* piece_entry = &pos->pieces[side][p][i];
                square_t from=piece_entry->location;
                const attack_data_t* attack_data = 
                    &get_attack_data(from, attacked_sq);
                if (from == attacker_sq ||
                        !(attack_data->possible_attackers &
                        get_piece_flag(piece_entry->piece))) continue;
                if (piece_slide_type(p) == NO_SLIDE) {
                    attackers[side][num_attackers[side]++] = piece_entry;
                    continue;
                }
                while (true) {
                    from += attack_data->relative_direction;
                    if (from == attacked_sq) {
                        attackers[side][num_attackers[side]++] = piece_entry;
                        break;
                    }
                    if (pos->board[from]) break;
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
        if (att_data->possible_attackers & get_piece_flag(QUEEN)) {
            square_t sq;
            for (sq=attacker_sq - att_data->relative_direction;
                    valid_board_index(sq) && !pos->board[sq];
                    sq -= att_data->relative_direction) {}
            if (valid_board_index(sq) && pos->board[sq]) {
                const piece_entry_t* xray = pos->board[sq];
                if (att_data->possible_attackers & 
                        get_piece_flag(xray->piece)) {
                    color_t xray_color = piece_color(xray->piece);
                    attackers[xray_color][num_attackers[xray_color]++] = xray;
                }
            }
        }

        // score the capture under the assumption that it's defended.
        gain[gain_index] = capture_value - gain[gain_index - 1];
        //printf("%d ", gain[gain_index]);
        //printf("(%c%c) ", (char)square_file(attacker_sq) + 'a', (char)square_rank(attacker_sq) + '1');
        ++gain_index;
        side ^= 1;

        // find the next lowest valued attacker
        int least_value = material_value(KING)+1;
        int att_index = -1;
        for (int i=0; i<num_attackers[side]; ++i) {
            capture_value = material_value(attackers[side][i]->piece);
            if (capture_value < least_value) {
                least_value = capture_value;
                att_index = i;
            }
        }
        if (att_index == -1) {
            assert(num_attackers[side] == 0);
            break;
        }
        attacker = attackers[side][att_index]->piece;
        attacker_sq = attackers[side][att_index]->location;
        attackers[side][att_index] = attackers[side][--num_attackers[side]];
        capture_value = least_value;
        if (piece_type(attacker) == KING && num_attackers[side^1]) break;
    }

    // Now that gain array is set up, scan back through to get score.
    --gain_index;
    while (--gain_index) {
        gain[gain_index-1] = -gain[gain_index-1] < gain[gain_index] ?
            -gain[gain_index] : gain[gain_index-1];
    }
    printf("\n");
    return gain[0];
}
