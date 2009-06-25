
#include "grasshopper.h"
#include <assert.h>

typedef enum {
    SSW=-33, SSE=-31,
    WSW=-18, SW=-17, S=-16, SE=-15, ESE=-14,
    W=-1, STATIONARY=0, E=1,
    WNW=14, NW=15, N=16, NE=17, ENE=18,
    NNW=31, NNE=33
} direction_t;

static const direction_t piece_deltas[16][16] = {
    // White Pieces
    {0},                                                    // Null
    {NW, NE, 0},                                            // Pawn
    {SSW, SSE, WSW, ESE, WNW, ENE, NNW, NNE, 0},            // Knight
    {SW, SE, NW, NE, 0},                                    // Bishop
    {S, W, E, N, 0},                                        // Rook
    {SW, S, SE, W, E, NW, N, NE, 0},                        // Queen
    {SW, S, SE, W, E, NW, N, NE, 0},                        // King
    {0}, {0},                                               // Null
    // Black Pieces
    {SE, SW, 0},                                            // Pawn
    {SSW, SSE, WSW, ESE, WNW, ENE, NNW, NNE, 0},            // Knight
    {SW, SE, NW, NE, 0},                                    // Bishop
    {S, W, E, N, 0},                                        // Rook
    {SW, S, SE, W, E, NW, N, NE, 0},                        // Queen
    {SW, S, SE, W, E, NW, N, NE, 0},                        // King
    {0}                                                     // Null
};


static void generate_pawn_moves(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves);
static void generate_piece_moves(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves);

void generate_moves(const position_t* pos, move_t* moves)
{
    color_t side = pos->side_to_move;
    for (piece_type_t type = KING; type != NONE; --type) {
        for (int i = 0; i < pos->piece_count[side][type]; ++i) {
            const piece_entry_t* piece_entry = &pos->pieces[side][type][i];
            switch (piece_type(piece_entry->piece)) {
                case PAWN: generate_pawn_moves(pos, piece_entry, &moves);
                           break;
                case KING:
                case KNIGHT:
                case BISHOP:
                case ROOK:
                case QUEEN: generate_piece_moves(pos, piece_entry, &moves);
                            break;
                default:    assert(false);
            }
        }
    }
    // FIXME: no castling or check recognition
}

static void generate_pawn_moves(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves_head)
{
    static const direction_t pawn_push[] = {N, S};
    static const rank_t relative_pawn_rank[2][8] = {
        {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8},
        {RANK_8, RANK_7, RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, RANK_1}
    };
    color_t side = pos->side_to_move;
    piece_t piece = piece_entry->piece;
    square_t from = piece_entry->location;
    square_t to;
    rank_t relative_rank = relative_pawn_rank[side][square_rank(from)];
    move_t* moves = *moves_head;
    if (relative_rank < RANK_7) {
        // non-promotions
        to = from + pawn_push[side];
        if (pos->board[to] == NULL) {
            *(moves++) = make_move(from, to, piece, EMPTY);
            to += pawn_push[side];
            if (relative_rank == RANK_2 && pos->board[to] == NULL) {
                // initial two-square push
                *(moves++) = make_move(from, to, piece, EMPTY);
            }
        }
        for (const uint32_t* delta = piece_deltas[piece]; *delta; ++delta) {
            // captures
            to = from + *delta;
            if (to == pos->ep_square) {
                *(moves++) = make_move_enpassant(from, to, piece,
                        pos->board[to + pawn_push[side^1]]->piece);
            }
            if (!valid_board_index(to) || !pos->board[to]) continue;
            if (piece_colors_differ(piece, pos->board[to]->piece)) {
                *(moves++) = make_move(from, to, piece, pos->board[to]->piece);
            }
        }
    } else {
        // promotions
        to = from + pawn_push[side];
        if (pos->board[to] == NULL) {
            for (piece_t promoted=QUEEN; promoted > PAWN; --promoted) {
                *(moves++) = make_move_promote(from, to, piece, EMPTY, promoted);
            }
        }
        for (const uint32_t* delta = piece_deltas[piece]; *delta; ++delta) {
            // capture/promotes
            to = from + *delta;
            if (!valid_board_index(to) || !pos->board[to]) continue;
            if (piece_colors_differ(piece, pos->board[to]->piece)) {
                for (piece_t promoted=QUEEN; promoted > PAWN; --promoted) {
                    *(moves++) = make_move_promote(from, to, piece,
                            pos->board[to]->piece, promoted);
                }
            }
        }
    }
    *moves_head = moves;
}

static void generate_piece_moves(const position_t* pos,
        const piece_entry_t* piece_entry,
        move_t** moves_head)
{
    const piece_t piece = piece_entry->piece;
    const square_t from = piece_entry->location;
    move_t* moves = *moves_head;
    square_t to;
    if (piece_slide_type(piece) == NONE) {
        // not a sliding piece, just iterate over dest. squares
        for (const uint32_t* delta = piece_deltas[piece]; *delta; ++delta) {
            to = from + *delta;
            if (!valid_board_index(to)) continue;
            if (pos->board[to] == NULL) {
                *(moves++) = make_move(from, to, piece, NONE);
            } else if (piece_colors_differ(piece, pos->board[to]->piece)) {
                *(moves++) = make_move(from, to, piece, pos->board[to]->piece);
            }
        }
    } else {
        // a sliding piece, keep going until we hit something
        for (const uint32_t* delta = piece_deltas[piece]; *delta; ++delta) {
            to = from;
            do {
                to += *delta;
                if (!valid_board_index(to)) break;
                if (pos->board[to] == NULL) {
                    *(moves++) = make_move(from, to, piece, NONE);
                } else if (piece_colors_differ(piece, pos->board[to]->piece)) {
                    *(moves++) = make_move(
                            from, to, piece, pos->board[to]->piece);
                }
            } while (!pos->board[to]);
        }
    }
    *moves_head = moves;
}

