
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
    for (square_t square=A8; square>=A1; ++fen, ++square) {
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
            case ' ': square = A1 - 2;
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
    while (*fen && !isspace(*fen)) {
        switch (*fen) {
            case 'q': pos->castle_rights |= BLACK_OOO; break;
            case 'Q': pos->castle_rights |= WHITE_OOO; break;
            case 'k': pos->castle_rights |= BLACK_OO; break;
            case 'K': pos->castle_rights |= WHITE_OO; break;
            default: 
                      // Either '-': no castle rights (do nothing), or
                      // A-Ha-h (960 castling notation).
                      // TODO: support A-Ha-h
                      if (*fen != '-') assert(false);
        }
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
