#ifndef DAYDREAMER_H
#define DAYDREAMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "compatibility.h"
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define ENGINE_NAME             "Daydreamer"
#ifndef ENGINE_VERSION
#define ENGINE_VERSION_NUMBER   "0.6"
#define ENGINE_VERSION_NAME
#define ENGINE_VERSION          ENGINE_VERSION_NUMBER ENGINE_VERSION_NAME
#endif
#define ENGINE_AUTHOR           "Aaron Becker"

#include "move.h"
#include "hash.h"
#include "position.h"
#include "eval.h"
#include "timer.h"
#include "search.h"
#include "trans_table.h"
#include "debug.h"

#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
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
bool is_endgame(const position_t* pos);

// hash.c
hashkey_t hash_position(const position_t* pos);

// io.c
void handle_console(position_t* pos, char* command);
int square_to_coord_str(square_t sq, char* str);
void move_to_la_str(move_t move, char* str);
void position_to_fen_str(const position_t* pos, char* fen);
void print_la_move(move_t move);
int print_la_move_list(const move_t* move);
void print_board(const position_t* pos);
void print_pv(search_data_t* search_data);
void check_for_input(search_data_t* search_data);

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
void copy_position(position_t* dst, const position_t* src);
bool is_square_attacked(const position_t* position,
        const square_t square,
        const color_t side);
bool is_move_legal(position_t* pos, const move_t move);
bool is_check(const position_t* pos);
bool is_repetition(const position_t* pos);

// san.c
int move_to_san_str(position_t* pos, move_t move, char* str);
int line_to_san_str(position_t* pos, move_t* line, char* san);

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
void increment_transposition_age(void);
transposition_entry_t* get_transposition_entry(position_t* pos);
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

