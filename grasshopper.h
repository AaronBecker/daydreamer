#ifndef GRASSHOPPER_H
#define GRASSHOPPER_H

#include <stdint.h>


typedef enum {
    INVALID=-1, WHITE=0, BLACK=1
} color_t;

typedef enum {
    WP=1, WN=2, WB=3, WR=4, WQ=5, WK=6,
    BP=9, BN=10, BB=11, BR=12, BQ=13, BK=14,
    EMPTY=16, INVALID=32
} piece_t;

typedef enum {
    NONE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
} piece_type_t;

#define piece_type(piece)               ((piece) & 0x07)
#define piece_is_type(piece, type)      (piece_type((piece)) == (type))
#define piece_color(piece)              ((piece) >> 3)
#define piece_is_color(piece, color)    (piece_color((piece)) == (color))
#define piece(color, type)              (((color) << 3) | (type))
#define piece_colors_match(p1, p2)      (((p1) >> 3) == ((p2) >> 3))
#define piece_colors_differ(p1, p2)     (!piece_colors_match((p1),(p2)))

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
    A8=0x70, B8=0x71, C8=0x72, D8=0x73, E8=0x74, F8=0x75, G8=0x76, H8=0x77
} square_t;

#define square_rank(square)     ((square) >> 4)
#define square_file(square)     ((square) & 0x0f)

/* 
 * Each move is a 4-byte quantity that encodes source and destination
 * square, the moved piece, any captured piece, the promotion value (if any),
 * and flags to indicate en-passant capture and castling.
 *
 * 12345678 12345678 123    45678       123         4   5           678
 * FROM     TO       PIECE  CAPTURE     PROMOTE     EP  CASTLE      UNUSED
 */
typedef move_t uint32_t;
#define ENPASSANT_FLAG          1<<27
#define CASTLE_FLAG             1<<28
#define move_from(move)         ((move) & 0xff)
#define move_to(move)           (((move) >> 8) & 0xff)
#define move_piece(move)        (((move) >> 16) & 0x07)
#define move_capture(move)      (((move) >> 19) & 0x07)
#define move_promote(move)      (((move) >> 24) & 0x07)
#define move_enpassant(move)    ((move) & ENPASSANT_FLAG)
#define move_castle(move)       ((move) & CASTLE_FLAG)
#define move_castle_long(move)  (move_castle(move) && \
                                    square_file(move_to(move)) == FILE_C)
#define move_castle_short(move) (move_castle(move) && \
                                    square_file(move_to(move)) == FILE_G)
#define move(from, to, piece, capture) \
                                ((from) | ((to) << 8) | ((piece) << 16) | \
                                 ((capture) << 19))
#define move(from, to, piece, capture, promote, flags) \
                                ((from) | ((to) << 8) | ((piece) << 16) | \
                                 ((capture) << 19) | ((promote) << 24) | flags)

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
    piece_entry_t* board[128];
    move_t prev_move;
    square_t ep_square;
    int fifty_move_counter;
    int ply;
    castle_rights_t castle_rights;
} position_t;

#endif

