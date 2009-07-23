#ifndef DAYDREAMER_H
#define DAYDREAMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "compatibility.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define ENGINE_NAME     "Daydreamer"
#define ENGINE_VERSION  "0.0"
#define ENGINE_AUTHOR   "Aaron Becker"

/**
 * Definitions for colors, pieces, and squares.
 */

#define FEN_STARTPOS "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

typedef enum {
    WHITE=0, BLACK=1, INVALID_COLOR=INT_MAX
} color_t;

typedef enum {
    EMPTY=0, WP=1, WN=2, WB=3, WR=4, WQ=5, WK=6,
    BP=9, BN=10, BB=11, BR=12, BQ=13, BK=14,
    INVALID_PIECE=16
} piece_t;

typedef enum {
    NONE=0, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
} piece_type_t;

#define piece_type(piece)               ((piece) & 0x07)
#define piece_is_type(piece, type)      (piece_type((piece)) == (type))
#define piece_color(piece)              ((piece) >> 3)
#define piece_is_color(piece, color)    (piece_color((piece)) == (color))
#define create_piece(color, type)       (((color) << 3) | (type))
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
    INVALID_SQUARE=0x4b // just some square from the middle of the invalid part
} square_t;

#define square_rank(square)         ((square) >> 4)
#define square_file(square)         ((square) & 0x0f)
#define create_square(file, rank)   (((rank) << 4) | (file))
#define valid_board_index(idx)      !(idx & 0x88)
#define flip_square(square)         ((square) ^ 0x70)
#define square_to_index(square)     ((square)+((square) & 0x07))>>1
#define index_to_square(square)     ((square)+((square) & ~0x07))

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
#define NO_MOVE                     0
#define NULL_MOVE                   0xffff
#define ENPASSANT_FLAG              1<<29
#define CASTLE_FLAG                 1<<30
#define get_move_from(move)         ((move) & 0xff)
#define get_move_to(move)           (((move) >> 8) & 0xff)
#define get_move_piece(move)        (((move) >> 16) & 0x0f)
#define get_move_capture(move)      (((move) >> 20) & 0x0f)
#define get_move_promote(move)      (((move) >> 24) & 0x0f)
#define is_move_enpassant(move)     (((move) & ENPASSANT_FLAG) != 0)
#define is_move_castle(move)        (((move) & CASTLE_FLAG) != 0)
#define is_move_castle_long(move) \
    (is_move_castle(move) && (square_file(get_move_to(move)) == FILE_C))
#define is_move_castle_short(move) \
    (is_move_castle(move) && (square_file(get_move_to(move)) == FILE_G))
#define create_move(from, to, piece, capture) \
    ((from) | ((to) << 8) | ((piece) << 16) | ((capture) << 20))
#define create_move_promote(from, to, piece, capture, promote) \
    ((from) | ((to) << 8) | ((piece) << 16) | \
     ((capture) << 20) | ((promote) << 24))
#define create_move_castle(from, to, piece) \
    ((from) | ((to) << 8) | ((piece) << 16) | CASTLE_FLAG)
#define create_move_enpassant(from, to, piece, capture) \
    ((from) | ((to) << 8) | ((piece) << 16) | \
     ((capture) << 20) | ENPASSANT_FLAG)

typedef enum {
    NO_SLIDE=0, DIAGONAL, STRAIGHT, BOTH
} slide_t;

extern const slide_t sliding_piece_types[];
#define piece_slide_type(piece)     (sliding_piece_types[piece])

/**
 * Definitions for positions.
 */
typedef struct {
    piece_t piece;
    square_t location;
} piece_entry_t;

typedef uint8_t castle_rights_t;
#define WHITE_OO                        0x01
#define BLACK_OO                        0x01 << 1
#define WHITE_OOO                       0x01 << 2
#define BLACK_OOO                       0x01 << 3
#define CASTLE_ALL                      (WHITE_OO | BLACK_OO | \
                                            WHITE_OOO | BLACK_OOO)
#define CASTLE_NONE                     0
#define has_oo_rights(pos, side)        ((pos)->castle_rights & \
                                            (WHITE_OO<<(side)))
#define has_ooo_rights(pos, side)       ((pos)->castle_rights & \
                                            (WHITE_OOO<<(side)))
#define add_oo_rights(pos, side)        ((pos)->castle_rights |= \
                                            (WHITE_OO<<(side)))
#define add_ooo_rights(pos, side)       ((pos)->castle_rights |= \
                                            (WHITE_OOO<<(side)))
#define remove_oo_rights(pos, side)     ((pos)->castle_rights &= \
                                            ~(WHITE_OO<<(side)))
#define remove_ooo_rights(pos, side)    ((pos)->castle_rights &= \
                                            ~(WHITE_OOO<<(side)))
typedef uint64_t hashkey_t;
#define HASH_HISTORY_LENGTH  512

typedef struct {
    piece_entry_t* board[128];          // 0x88 board
    piece_entry_t pieces[2][8][16];     // [color][type][piece entries]
    int piece_count[2][8];              // [color][type]
    color_t side_to_move;
    move_t prev_move;
    square_t ep_square;
    int fifty_move_counter;
    int ply;
    int material_eval[2];
    int piece_square_eval[2];
    castle_rights_t castle_rights;
    hashkey_t hash;
    hashkey_t hash_history[HASH_HISTORY_LENGTH];
} position_t;

typedef struct {
    move_t prev_move;
    square_t ep_square;
    int fifty_move_counter;
    castle_rights_t castle_rights;
    hashkey_t hash;
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

extern const direction_t pawn_push[];
extern const rank_t relative_pawn_rank[2][8];

typedef enum {
    NONE_FLAG=0, WP_FLAG=1<<0, BP_FLAG=1<<1, N_FLAG=1<<2,
    B_FLAG=1<<3, R_FLAG=1<<4, Q_FLAG=1<<5, K_FLAG=1<<6
} piece_flag_t;
extern const piece_flag_t piece_flags[];
#define get_piece_flag(piece)       piece_flags[(piece)]

typedef struct {
    piece_flag_t possible_attackers;
    direction_t relative_direction;
} attack_data_t;
// An array indexed from -128 to 128 which stores attack data for every pair
// of squares (to, from) at the index [from-to].
extern const attack_data_t* board_attack_data;
#define get_attack_data(from, to)   board_attack_data[(from)-(to)]

typedef struct {
    struct timeval tv_start;
    int elapsed_millis;
    bool running;
} milli_timer_t;

/*
 * Position evaluation.
 */
extern int piece_square_values[BK+1][0x80];
extern const int material_values[];
#define material_value(piece)               material_values[piece]
#define piece_square_value(piece, square)   piece_square_values[piece][square]

/*
 * Search data.
 */
#define MAX_SEARCH_DEPTH        64

typedef struct {
    move_t pv[MAX_SEARCH_DEPTH];
} search_node_t;

typedef enum {
    ENGINE_IDLE=0, ENGINE_PONDERING, ENGINE_THINKING, ENGINE_ABORTED
} engine_status_t;

typedef enum {
    SCORE_EXACT, SCORE_LOWERBOUND, SCORE_UPPERBOUND
} score_type_t;

typedef struct {
    int poll_interval;
    int output_delay;
} search_options_t;

typedef struct {
    position_t root_pos;
    search_options_t options;

    // search state info
    move_t root_moves[256];
    move_t best_move; // FIXME: shouldn't this be redundant with pv[0]?
    int best_score;
    move_t pv[MAX_SEARCH_DEPTH];
    search_node_t search_stack[MAX_SEARCH_DEPTH];
    uint64_t nodes_searched;
    int current_depth;
    engine_status_t engine_status;

    // when should we stop?
    milli_timer_t timer;
    uint64_t node_limit;
    int depth_limit;
    int time_limit;
    int time_target;
    int mate_search;
    bool infinite;
    bool ponder;
} search_data_t;

extern search_data_t root_data;

#define POLL_INTERVAL   0xffff
#define MATE_VALUE      0x7fff
#define DRAW_VALUE      0
#define NULL_R          3
#define NULLMOVE_DEPTH_REDUCTION    4

#define is_mate_score(score)       (abs(score) + 256 > MATE_VALUE)


/*
 * Hashing.
 */

extern const hashkey_t piece_random[2][7][64];
extern const hashkey_t castle_random[2][2][2];
extern const hashkey_t enpassant_random[64];
extern const hashkey_t side_random[2];
#define piece_hash(p,sq) \
    piece_random[piece_color(p)][piece_type(p)][square_to_index(sq)]
#define ep_hash(pos) \
    enpassant_random[square_to_index((pos)->ep_square)]
#define castle_hash(pos) \
    (castle_random[has_oo_rights(pos, WHITE) ? 1 : 0][0][0] ^ \
    castle_random[has_ooo_rights(pos, WHITE) ? 1 : 0][0][1] ^ \
    castle_random[has_oo_rights(pos, BLACK) ? 1 : 0][1][0] ^ \
    castle_random[has_ooo_rights(pos, BLACK) ? 1 : 0][1][1])
#define side_hash(pos)  ((pos)->side_to_move * 0x629b66dbf31b7adbull)

typedef struct {
    hashkey_t key;
    move_t move;
    uint16_t depth;
    int32_t score;
    score_type_t score_type;
} transposition_entry_t;
extern transposition_entry_t* transposition_table;

/**
 * Debugging functionality.
 */

void _check_board_validity(const position_t* pos);
void _check_move_validity(const position_t* pos, const move_t move);
void _check_position_hash(const position_t* pos);
void _check_line(position_t* pos, move_t* line);

#ifdef OMIT_CHECKS
#define check_board_validity(x)                 ((void)0)
#define check_move_validity(x,y)                ((void)0,(void)0)
#define check_position_hash(x)                  ((void)0)
#define check_line(x,y)                         ((void)0)
#else
#define check_board_validity(x)                 _check_board_validity(x)
#define check_move_validity(x,y)                _check_move_validity(x,y)
#define check_position_hash(x)                  _check_position_hash(x)
#define check_line(x,y)                         _check_line(x,y)
#endif

/**
 * External function interface
 */

// daydreamer.c
void init_daydreamer(void);
void generate_attack_data(void);

// eval.c
int simple_eval(const position_t* pos);
bool insufficient_material(const position_t* pos);
bool is_draw(const position_t* pos);

// hash.c
hashkey_t hash_position(const position_t* pos);

// io.c
void handle_console(position_t* pos, char* command);
void move_to_la_str(move_t move, char* str);
void position_to_fen_str(position_t* pos, char* fen);
void print_la_move(move_t move);
int print_la_move_list(const move_t* move);
void print_board(const position_t* pos);
void print_pv(search_data_t* search_data);
void check_for_input(search_data_t* search_data);
// unimplemented
void move_to_san_str(position_t* pos, move_t move, char* str);

// move.c
void place_piece(position_t* position,
        const piece_t piece,
        const square_t square);
void remove_piece(position_t* position,
        const square_t square);
void transfer_piece(position_t* position,
        const square_t from,
        const square_t to);
void do_move(position_t* position, const move_t move, undo_info_t* undo);
void undo_move(position_t* position, const move_t move, undo_info_t* undo);
void do_nullmove(position_t* pos, undo_info_t* undo);
void undo_nullmove(position_t* pos, undo_info_t* undo);

// move_generation.c
int generate_legal_moves(const position_t* pos, move_t* moves);
int generate_legal_noncaptures(const position_t* pos, move_t* moves);
int generate_pseudo_moves(const position_t* position, move_t* move_list);
int generate_pseudo_captures(const position_t* position, move_t* move_list);
int generate_pseudo_noncaptures(const position_t* position, move_t* move_list);

// parse.c
move_t parse_la_move(position_t* pos, const char* la_move);
square_t parse_la_square(const char* la_square);

// perft.c
void perft_testsuite(char* filename);
uint64_t perft(position_t* position, int depth, bool divide);

// position.c
char* set_position(position_t* position, const char* fen);
void copy_position(position_t* dst, position_t* src);
bool is_square_attacked(const position_t* position,
        const square_t square,
        const color_t side);
bool is_move_legal(position_t* pos, const move_t move);
bool is_check(const position_t* pos);
bool is_repetition(const position_t* pos, int n);

// search.c
void init_search_data(search_data_t* data);
void root_search_minimax(void);
void deepening_search(search_data_t* search_data);

// static_exchange_eval.c
int static_exchange_eval(position_t* pos, move_t move);

// timer.c
void init_timer(milli_timer_t* timer);
void start_timer(milli_timer_t* timer);
int stop_timer(milli_timer_t* timer);
int elapsed_time(milli_timer_t* timer);

// trans_table.c
void init_transposition_table(const int max_bytes);
void clear_transposition_table(void);
bool get_transposition(position_t* pos,
        int depth,
        int* lb,
        int* ub,
        move_t* move);
void put_transposition(position_t* pos,
        move_t move,
        int depth,
        int score,
        score_type_t score_type);
void print_transposition_stats(void);

// uci.c
void uci_main(void);

// uci_option.c
void init_uci_options(search_options_t* options);
void set_uci_option(char* command, search_options_t* options);
void print_uci_options(void);


#ifdef __cplusplus
} // extern "C"
#endif
#endif // DAYDREAMER_H

