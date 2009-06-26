
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include "grasshopper.h"

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
                entry->piece = make_piece(color, type);
                entry->location = INVALID_SQUARE;
            }
        }
    }

    position->fifty_move_counter = 0;
    position->ep_square = INVALID_SQUARE;
    position->ply = 0;
    position->side_to_move = WHITE;
}

static square_t read_alg_square(const char* alg_square)
{
    assert(alg_square[0] >= 'a' && alg_square[0] < 'i');
    assert(alg_square[1] > '0' && alg_square[1] < '9'); 
    // TODO: log warning instead.
    return make_square(tolower(alg_square[0])-'a', alg_square[1]-'1');
}

void set_position(position_t* pos, const char* fen)
{
    init_position(pos);

    // Read piece positions.
    for (square_t square=A8; square!=INVALID_SQUARE; ++fen, ++square) {
        if (isdigit(*fen)) {
            if (*fen == '0' || *fen == '9') {
                //TODO: log_warning
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
            case '\0': return;
            default: assert(false); //TODO: log_warning
        }
    }
    while (isspace(*fen)) ++fen;

    // Read whose turn is next.
    switch (tolower(*fen)) {
        case 'w': pos->side_to_move = WHITE; break;
        case 'b': pos->side_to_move = BLACK; break;
        default: assert(false); // TODO: log_warning
    }
    while (*fen && isspace(*(++fen))) {}

    // Read castling rights.
    pos->castle_rights = CASTLE_NONE;
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
                      if (*fen != '-') assert(false);
        }
        ++fen;
    }
    while (isspace(*fen)) ++fen;
    if (!*fen) return;

    // Read en-passant square
    if (*fen != '-') {
        pos->ep_square = read_alg_square(fen++);
    }
    while (*fen && isspace(*(++fen))) {}
    if (!*fen) return;

    // Read 50-move rule status and current move number.
    sscanf(fen, "%d %d", &pos->fifty_move_counter, &pos->ply);
    pos->ply = pos->ply*2 + (pos->side_to_move == BLACK ? 1 : 0);
}

bool is_square_attacked(const position_t* pos, const square_t sq, const color_t side)
{
    color_t other_side = side^1;
    // For every opposing piece, look up the attack data for its square.
    for (piece_t p=PAWN; p<=KING; ++p) {
        for (int i=0; i<pos->piece_count[other_side][p]; ++i) {
            square_t from=pos->pieces[other_side][p][i].location;
            const attack_data_t* attack_data = &get_attack_data(from, sq);
            if (attack_data->possible_attackers | get_piece_flag(p)) {
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

bool is_move_legal(const position_t* pos, const move_t move)
{
    color_t other_side = pos->side_to_move^1;
    square_t king_square = pos->pieces[pos->side_to_move][KING][0].location;
    if (get_move_piece(move) == KING) {
        return !is_square_attacked(pos, get_move_to(move), other_side);
    }

    // See if any attacks are preventing castling.
    if (is_move_castle_long(move)) {
        return (is_square_attacked(pos, king_square, other_side) ||
                is_square_attacked(pos, king_square-1, other_side) ||
                is_square_attacked(pos, king_square-2, other_side));
    }
    if (is_move_castle_short(move)) {
        return (is_square_attacked(pos, king_square, other_side) ||
                is_square_attacked(pos, king_square+1, other_side) ||
                is_square_attacked(pos, king_square+2, other_side));
    }

    // Just try the move and see if the king is being attacked afterwards.
    // This is sort of inefficient--actually making and unmaking the move isn't strictly
    // necessary.
    undo_info_t undo;
    do_move((position_t*)pos, move, &undo);
    bool legal = !is_square_attacked(pos, get_move_to(move), other_side);
    undo_move((position_t*)pos, move, &undo);

    return legal;
}
