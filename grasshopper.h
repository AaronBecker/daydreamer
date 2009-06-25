#ifndef GRASSHOPPER_H
#define GRASSHOPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Definitions for colors, pieces, and squares.
 */

typedef enum {
    INVALID_COLOR=-1, WHITE=0, BLACK=1
} color_t;

typedef enum {
    EMPTY=0, WP=1, WN=2, WB=3, WR=4, WQ=5, WK=6,
    BP=9, BN=10, BB=11, BR=12, BQ=13, BK=14,
    INVALID_PIECE=16
} piece_t;

typedef enum {
    NONE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
} piece_type_t;

#define piece_type(piece)               ((piece) & 0x07)
#define piece_is_type(piece, type)      (piece_type((piece)) == (type))
#define piece_color(piece)              ((piece) >> 3)
#define piece_is_color(piece, color)    (piece_color((piece)) == (color))
#define make_piece(color, type)         (((color) << 3) | (type))
#define piece_colors_match(p1, p2)      (((p1) >> 3) == ((p2) >> 3))
#define piece_colors_differ(p1, p2)     (((p1) >> 3) != ((p2) >> 3))

typedef enum {
    FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H
} file_t;

typedef enum {
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8
} rank_t;

typedef enum {
    A1=0x00, B1=0x01, C1=0x02, D1=0x03, E1=0x04, F1=0x05, G1=0x06, H1=0x07,
    A2=0x10, B2=0x11, C2=0x12, D2=0x13, E2=0x14, F2=0x15, G2=0x16, H2=0x17,
    A3=0x20, B3=0x21, C3=0x22, D3=0x23, E3=0x24, F3=0x25, G3=0x26, H3=0x27,
    A4=0x30, B4=0x31, C4=0x32, D4=0x33, E4=0x34, F4=0x35, G4=0x36, H4=0x37,
    A5=0x40, B5=0x41, C5=0x42, D5=0x43, E5=0x44, F5=0x45, G5=0x46, H5=0x47,
    A6=0x50, B6=0x51, C6=0x52, D6=0x53, E6=0x54, F6=0x55, G6=0x56, H6=0x57,
    A7=0x60, B7=0x61, C7=0x62, D7=0x63, E7=0x64, F7=0x65, G7=0x66, H7=0x67,
    A8=0x70, B8=0x71, C8=0x72, D8=0x73, E8=0x74, F8=0x75, G8=0x76, H8=0x77,
    INVALID_SQUARE=0xff
} square_t;

#define square_rank(square)     ((square) >> 4)
#define square_file(square)     ((square) & 0x0f)
#define make_square(file, rank) (((rank) << 4) & (file))
#define valid_board_index(idx)  !(idx & 0x88)

/**
 * Definitions for moves.
 * Each move is a 4-byte quantity that encodes source and destination
 * square, the moved piece, any captured piece, the promotion value (if any),
 * and flags to indicate en-passant capture and castling. The bit layout of
 * a move is as follows:
 *
 * 12345678  12345678  1234     5678     1234          5    6       78
 * FROM      TO        PIECE    CAPTURE  PROMOTE       EP   CASTLE  UNUSED
 * square_t  square_t  piece_t  piece_t  piece_type_t  bit  bit
 */
typedef uint32_t move_t;
#define ENPASSANT_FLAG          1<<29
#define CASTLE_FLAG             1<<30
#define get_move_from(move)         ((move) & 0xff)
#define get_move_to(move)           (((move) >> 8) & 0xff)
#define get_move_piece(move)        (((move) >> 16) & 0x0f)
#define get_move_capture(move)      (((move) >> 20) & 0x0f)
#define get_move_promote(move)      (((move) >> 24) & 0x0f)
#define get_move_enpassant(move)    ((move) & ENPASSANT_FLAG)
#define get_move_castle(move)       ((move) & CASTLE_FLAG)
#define is_move_castle_long(move)   (move_castle(move) && \
                                        square_file(get_move_to(move)) == FILE_C)
#define is_castle_short(move)       (move_castle(move) && \
                                        square_file(get_move_to(move)) == FILE_G)
#define create_move(from, to, piece, capture) \
                                ((from) | ((to) << 8) | ((piece) << 16) | \
                                 ((capture) << 20))
#define create_move_promote(from, to, piece, capture, promote) \
                                ((from) | ((to) << 8) | ((piece) << 16) | \
                                 ((capture) << 20) | ((promote) << 24))
#define create_move_castle(from, to, piece) \
                                ((from) | ((to) << 8) | ((piece) << 16) | \
                                 CASTLE_FLAG)
#define create_move_enpassant(from, to, piece, capture) \
                                ((from) | ((to) << 8) | ((piece) << 16) | \
                                 ((capture) << 20) | ENPASSANT_FLAG)

typedef enum {
    NO_SLIDE=0, DIAGONAL, STRAIGHT, BOTH
} slide_t;

extern const int sliding_piece_types[];
#define piece_slide_type(piece)     (sliding_piece_types[piece])

/**
 * Definitions for positions.
 */

typedef struct {
    piece_t piece;
    square_t location;
} piece_entry_t;

typedef uint8_t castle_rights_t;
#define WHITE_OO                0x01
#define WHITE_OOO               0x01 << 1
#define BLACK_OO                0x01 << 2
#define BLACK_OOO               0x01 << 3

typedef struct {
    piece_entry_t* board[128];          // 0x88 board
    piece_entry_t pieces[2][8][16];     // [color][type][piece entries]
    uint8_t piece_count[2][8];          // [color][type]
    color_t side_to_move;
    move_t prev_move;
    square_t ep_square;
    int fifty_move_counter;
    int ply;
    castle_rights_t castle_rights;
} position_t;

typedef struct {
    square_t ep_square;
    int fifty_move_counter;
    castle_rights_t castle_rights;
} undo_info_t;

/*
 * Definitions for piece movement and attack directions.
 */

typedef enum {
    SSW=-33, SSE=-31,
    WSW=-18, SW=-17, S=-16, SE=-15, ESE=-14,
    W=-1, STATIONARY=0, E=1,
    WNW=14, NW=15, N=16, NE=17, ENE=18,
    NNW=31, NNE=33
} direction_t;
extern const direction_t piece_deltas[16][16];

typedef enum {
    NONE_FLAG=0, WP_FLAG=1<<0, BP_FLAG=1<<1, N_FLAG=1<<2,
    B_FLAG=1<<3, R_FLAG=1<<4, Q_FLAG=1<<5, K_FLAG=1<<6
} piece_flag_t;
extern const piece_flag_t piece_flags[];


typedef struct {
    piece_flag_t possible_attackers;
    direction_t relative_direction;
} attack_data_t;
// An array indexed from -128 to 128 which stores attack data for every pair
// of squares (to, from) at the index [from-to].
extern const attack_data_t* board_attack_data;

/**
 * External function interface
 */

// position.c
void set_position(position_t* position, const char* fen);
move_t parse_move(position_t* position, const char* move_str);

// move.c
void place_piece(position_t* position, piece_t piece, square_t square);
void remove_piece(position_t* position, square_t square);
void transfer_piece(position_t* position, square_t from, square_t to);
// unimplemented
void do_move(position_t* position, move_t move);
void undo_move(position_t* position, move_t move, undo_info_t* undo);

// move_generation.c
void generate_moves(const position_t* position, move_t* move_list);

// io.c
void move_to_la_str(move_t move, char* str);
void print_la_move(move_t move);
void print_la_move_list(const move_t* move);
void print_board(const position_t* pos);
// unimplemented
void move_to_san_str(position_t* pos, move_t move, char* str);
void position_to_fen_str(position_t* pos, char* str);


#ifdef __cplusplus
} // extern "C"
#endif
#endif

