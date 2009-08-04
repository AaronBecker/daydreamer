
#include "daydreamer.h"
#include <string.h>
#include <stdio.h>

#define AMBIG_NONE  0x00
#define AMBIG_RANK  0x01
#define AMBIG_FILE  0x02
typedef int ambiguity_t;

static char piece_chars[] = " PNBRQK";
#define piece_type_char(x) piece_chars[x]

static ambiguity_t determine_move_ambiguity(position_t* pos, move_t move)
{
    square_t dest = get_move_to(move);
    rank_t from_rank = square_rank(get_move_from(move));
    file_t from_file = square_file(get_move_from(move));
    piece_type_t type = get_move_piece_type(move);
    move_t moves[256];
    generate_legal_moves(pos, moves);
    ambiguity_t ambiguity = AMBIG_NONE;
    for (move_t* other_move = moves; *other_move; ++other_move) {
        if (*other_move == move) continue;
        if (get_move_to(*other_move) != dest) continue;
        if (get_move_piece_type(*other_move) != type) continue;
        square_t other_from = get_move_from(*other_move);
        if (square_rank(other_from) == from_rank) ambiguity |= AMBIG_RANK;
        if (square_file(other_from) == from_file) ambiguity |= AMBIG_FILE;
    }
    return ambiguity;
}

int move_to_san_str(position_t* pos, move_t move, char* san)
{
    char* orig_san = san;
    if (move == NO_MOVE) {
        strcpy(san, "(none)");
        return 6;
    } else if (is_move_castle_short(move)) {
        strcpy(san, "O-O");
        san += 3;
    } else if (is_move_castle_long(move)) {
        strcpy(san, "O-O-O");
        san += 5;
    } else {
        // type
        piece_type_t type = get_move_piece_type(move);
        if (type != PAWN) {
            *san++ = piece_type_char(type);
        } else if (get_move_capture(move)) {
            *san++ = square_file(get_move_to(move)) + 'a';
        }
        // source
        ambiguity_t ambiguity = determine_move_ambiguity(pos, move);
        square_t from = get_move_from(move);
        if (ambiguity & AMBIG_RANK) *san++ = square_file(from) + 'a';
        if (ambiguity & AMBIG_FILE) *san++ = square_rank(from) + '1';
        // destination
        if (get_move_capture(move)) *san++ = 'x';
        san += square_to_coord_str(get_move_to(move), san);
        // promotion
        piece_type_t promote_type = get_move_promote(move);
        if (promote_type) {
            *san++ = '=';
            *san++ = piece_type_char(promote_type);
        }
    }
    // check or checkmate
    undo_info_t undo;
    do_move(pos, move, &undo);
    if (is_check(pos)) {
        move_t moves[256];
        int legal_moves = generate_legal_moves(pos, moves);
        *san++ = legal_moves ? '+' : '#';
    }
    undo_move(pos, move, &undo);
    *san = '\0';
    return san-orig_san;
}

int line_to_san_str(position_t* pos, move_t* line, char* san)
{
    if (!*line) {
        *san = '\0';
        return 0;
    }
    int len = move_to_san_str(pos, *line, san);
    *(san+len) = ' ';
    undo_info_t undo;
    do_move(pos, *line, &undo);
    len += line_to_san_str(pos, line+1, san+len+1);
    undo_move(pos, *line, &undo);
    return len;
}
