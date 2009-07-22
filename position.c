
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "daydreamer.h"

/*
 * Set up the basic data structures of a position. Used internally by
 * set_position, but does not result in a legal board and should not be used
 * elsewhere.
 */
static void init_position(position_t* position)
{
    for (int square=0; square<128; ++square) {
        position->board[square] = NULL;
    }

    for (color_t color=WHITE; color<=BLACK; ++color) {
        for (piece_type_t type=PAWN; type<=KING; ++type) {
            position->piece_count[color][type] = 0;
            for (int index=0; index<16; ++index) {
                piece_entry_t* entry = &position->pieces[color][type][index];
                entry->piece = create_piece(color, type);
                entry->location = INVALID_SQUARE;
            }
        }
        position->material_eval[color] = 0;
        position->piece_square_eval[color] = 0;
    }

    position->fifty_move_counter = 0;
    position->ep_square = EMPTY;
    position->ply = 0;
    position->side_to_move = WHITE;
    position->castle_rights = CASTLE_NONE;
    position->prev_move = NO_MOVE;
    position->hash = 0;
    memset(position->hash_history, 0, HASH_HISTORY_LENGTH * sizeof(hashkey_t));
}

/*
 * Create a copy of |src| in |dst|.
 */
void copy_position(position_t* dst, position_t* src)
{
    check_board_validity(src);
    memcpy(dst, src, sizeof(position_t));
    for (color_t color=WHITE; color<=BLACK; ++color) {
        for (piece_type_t type=PAWN; type<=KING; ++type) {
            for (int index=0; index<dst->piece_count[color][type]; ++index) {
                piece_entry_t* entry = &dst->pieces[color][type][index];
                if (entry->location != INVALID_SQUARE) {
                    dst->board[entry->location] = entry;
                }
            }
        }
    }
    check_board_validity(src);
    check_board_validity(dst);
}

/*
 * Given an FEN position description, set the given position to match it.
 * (see wikipedia.org/wiki/Forsyth-Edwards_Notation for a description of FEN)
 */
char* set_position(position_t* pos, const char* fen)
{
    init_position(pos);

    // Read piece positions.
    for (square_t square=A8; square!=INVALID_SQUARE; ++fen, ++square) {
        if (isdigit(*fen)) {
            if (*fen == '0' || *fen == '9') {
                assert(false);
            } else {
                square += *fen - '1';
            }
            continue;
        }
        switch (*fen) {
            case 'p': place_piece(pos, BP, square); break;
            case 'P': place_piece(pos, WP, square); break;
            case 'n': place_piece(pos, BN, square); break;
            case 'N': place_piece(pos, WN, square); break;
            case 'b': place_piece(pos, BB, square); break;
            case 'B': place_piece(pos, WB, square); break;
            case 'r': place_piece(pos, BR, square); break;
            case 'R': place_piece(pos, WR, square); break;
            case 'q': place_piece(pos, BQ, square); break;
            case 'Q': place_piece(pos, WQ, square); break;
            case 'k': place_piece(pos, BK, square); break;
            case 'K': place_piece(pos, WK, square); break;
            case '/': square -= 17 + square_file(square); break;
            case ' ': square = INVALID_SQUARE-1; break;
            case '\0':
            case '\n':check_board_validity(pos);
                      pos->hash = hash_position(pos);
                      return (char*)fen;
            default: assert(false);
        }
    }
    while (isspace(*fen)) ++fen;

    // Read whose turn is next.
    switch (tolower(*fen)) {
        case 'w': pos->side_to_move = WHITE; break;
        case 'b': pos->side_to_move = BLACK; break;
        default: assert(false);
    }
    while (*fen && isspace(*(++fen))) {}

    // Read castling rights.
    while (*fen && !isspace(*fen)) {
        switch (*fen) {
            case 'q': add_ooo_rights(pos, BLACK); break;
            case 'Q': add_ooo_rights(pos, WHITE); break;
            case 'k': add_oo_rights(pos, BLACK); break;
            case 'K': add_oo_rights(pos, WHITE); break;
            default: 
                      // Either '-': no castle rights (do nothing), or
                      // A-Ha-h (960 castling notation).
                      // TODO: support A-Ha-h
                      if (*fen != '-') {
                          // the fen string must have ended
                          check_board_validity(pos);
                          pos->hash = hash_position(pos);
                          return (char*)fen;
                      }
        }
        ++fen;
    }
    while (isspace(*fen)) ++fen;
    if (!*fen) {
        check_board_validity(pos);
        pos->hash = hash_position(pos);
        return (char*)fen;
    }

    // Read en-passant square
    if (*fen != '-') {
        pos->ep_square = parse_la_square(fen++);
    }
    while (*fen && isspace(*(++fen))) {}
    if (!*fen) {
        pos->hash = hash_position(pos);
        check_board_validity(pos);
        return (char*)fen;
    }

    // Read 50-move rule status and current move number.
    int consumed;
    if (sscanf(fen, "%d %d%n", &pos->fifty_move_counter, &pos->ply,
                &consumed)) fen += consumed;
    pos->ply = pos->ply*2 + (pos->side_to_move == BLACK ? 1 : 0);
    pos->hash = hash_position(pos);
    check_board_validity(pos);
    return (char*)fen;
}

bool is_square_attacked(const position_t* pos,
        const square_t sq,
        const color_t side)
{
    // For every opposing piece, look up the attack data for its square.
    for (piece_t p=PAWN; p<=KING; ++p) {
        const int num_pieces = pos->piece_count[side][p];
        for (int i=0; i<num_pieces; ++i) {
            const piece_entry_t* piece_entry = &pos->pieces[side][p][i];
            square_t from=piece_entry->location;
            const attack_data_t* attack_data = &get_attack_data(from, sq);
            if (attack_data->possible_attackers &
                    get_piece_flag(piece_entry->piece)) {
                if (piece_slide_type(p) == NO_SLIDE) return true;
                while (from != sq) {
                    from += attack_data->relative_direction;
                    if (from == sq) return true;
                    if (pos->board[from]) break;
                }
            }
        }
    }
    return false;
}

bool is_check(const position_t* pos)
{
    square_t king_square = pos->pieces[pos->side_to_move][KING][0].location;
    return is_square_attacked(pos, king_square, pos->side_to_move^1);
}

bool is_move_legal(position_t* pos, const move_t move)
{
    const color_t other_side = pos->side_to_move^1;
    const square_t king_square = pos->pieces[other_side^1][KING][0].location;

    // don't try to make invalid/inconsistent moves
    const square_t from = get_move_from(move);
    const square_t to = get_move_to(move);
    const piece_t piece = get_move_piece(move);
    const piece_t capture = get_move_capture(move);
    if (!valid_board_index(from) || !valid_board_index(to)) return false;
    if (!pos->board[from] || pos->board[from]->piece != piece) return false;
    if (pos->board[from]->location != from) return false;
    if (capture && !is_move_enpassant(move)) {
        if (!pos->board[to] || pos->board[to]->piece != capture) return false;
    } else {
        if (pos->board[to] != NULL) return false;
    }

    // See if any attacks are preventing castling.
    if (is_move_castle_long(move)) {
        return !(is_square_attacked(pos, king_square, other_side) ||
                is_square_attacked(pos, king_square-1, other_side) ||
                is_square_attacked(pos, king_square-2, other_side));
    }
    if (is_move_castle_short(move)) {
        return !(is_square_attacked(pos, king_square, other_side) ||
                is_square_attacked(pos, king_square+1, other_side) ||
                is_square_attacked(pos, king_square+2, other_side));
    }

    // Just try the move and see if the king is being attacked afterwards.
    // This is sort of inefficient--actually making and unmaking the move
    // isn't strictly necessary.
    undo_info_t undo;
    do_move(pos, move, &undo);
    bool legal = !is_square_attacked(pos,
            pos->pieces[pos->side_to_move^1][KING][0].location,
            other_side);
    undo_move(pos, move, &undo);
    return legal;
}

bool is_repetition(const position_t* pos)
{
    if (pos->fifty_move_counter < 2) return false;
    int ply_distance = 2;
    while (ply_distance <= pos->fifty_move_counter) {
        assert(pos->ply >= ply_distance);
        if (pos->hash_history[pos->ply-ply_distance] == pos->hash) return true;
        ply_distance += 2;
    }
    return false;
}
