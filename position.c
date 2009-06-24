
#include <ctype.h>
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
}

void set_position(position_t* position, const char* fen)
{
    init_position(position);
    for (square_t square=A8; square>=A1; ++fen) {
        if (isdigit(*fen)) {
            if (*fen == '0' || *fen == '9') {
                //TODO: log_warning
            } else {
                square += *fen - '0';
            }
            continue;
        }
        switch (*fen) {
            case 'p': break;
        // FIXME: unimplemented
        }
    }
}
