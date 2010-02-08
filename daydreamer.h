#ifndef DAYDREAMER_H
#define DAYDREAMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "compatibility.h"
#include <limits.h>
#include <stdlib.h>

#ifndef GIT_VERSION
#define GIT_VERSION
#endif
#define ENGINE_NAME             "Daydreamer"
#define ENGINE_VERSION_NUMBER   "1.61"
#define ENGINE_VERSION_NAME     " ffrac7_" GIT_VERSION
#define ENGINE_VERSION          ENGINE_VERSION_NUMBER ENGINE_VERSION_NAME
#define ENGINE_AUTHOR           "Aaron Becker"

#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef CLAMP
#define CLAMP(x, low, high) (((x)>(high)) ? (high):(((x)<(low)) ? (low):(x)))
#endif

#include "board.h"
#include "bitboard.h"
#include "move.h"
#include "hash.h"
#include "eval.h"
#include "position.h"
#include "attack.h"
#include "timer.h"
#include "search.h"
#include "trans_table.h"
#include "pawn.h"
#include "move_selection.h"
#include "eval_material.h"
#include "debug.h"

/*
 * External function interface
 */

// attack.c
void generate_attack_data(void);
direction_t pin_direction(const position_t* pos,
        square_t from,
        square_t king_sq);
bool is_square_attacked(const position_t* pos, square_t square, color_t side);
bool piece_attacks_near(const position_t* pos, square_t from, square_t target);
uint8_t find_checks(position_t* pos);

// benchmark.c
void benchmark(int depth, int time_limit);

// bitboard.c
void init_bitboards(void);
void print_bitboard(bitboard_t bb);

// book.c
void init_book(char* filename);
move_t get_book_move(position_t* pos);
void test_book(char* filename, position_t* pos);

// daydreamer.c
void init_daydreamer(void);

// egbb.c
bool load_egbb(char* egbb_dir, int cache_size_bytes);
void unload_egbb(void);
bool probe_egbb(position_t* pos, int* value, int ply);

// epd.c
void epd_testsuite(char* filename, int time_per_problem);

// eval.c
void init_eval(void);
int simple_eval(const position_t* pos);
int full_eval(const position_t* pos, eval_data_t* ed);
void report_eval(const position_t* pos);
bool insufficient_material(const position_t* pos);
bool can_win(const position_t* pos, color_t side);
bool is_draw(const position_t* pos);
int game_phase(const position_t* pos);

// eval_endgame.c
bool endgame_score(const position_t* pos, eval_data_t* ed, int* score);
void determine_endgame_scale(const position_t* pos,
        eval_data_t* ed,
        int endgame_scale[2]);

// eval_material.c
void init_material_table(const int max_bytes);
void clear_material_table(void);
material_data_t* get_material_data(const position_t* pos);

// eval_patterns.c
score_t pattern_score(const position_t*pos);

// eval_pawns.c
void init_pawn_table(const int max_bytes);
void clear_pawn_table(void);
score_t pawn_score(const position_t* pos, pawn_data_t** pawn_data);
void print_pawn_stats(void);

// eval_pieces.c
score_t pieces_score(const position_t* pos, pawn_data_t* pd);

// format.c
int square_to_coord_str(square_t sq, char* str);
square_t coord_str_to_square(const char* coord_square);
void move_to_coord_str(move_t move, char* str);
move_t coord_str_to_move(position_t* pos, const char* coord_move);
int move_to_san_str(position_t* pos, move_t move, char* str);
int line_to_san_str(position_t* pos, move_t* line, char* san);
move_t san_str_to_move(position_t* pos, char* san);
void position_to_fen_str(const position_t* pos, char* fen);

// hash.c
void init_hash(void);
int get_hashfull(void);
hashkey_t hash_position(const position_t* pos);
hashkey_t hash_pawns(const position_t* pos);
hashkey_t hash_material(const position_t* pos);

// move.c
void place_piece(position_t* position, piece_t piece, square_t square);
void remove_piece(position_t* position, square_t square);
void transfer_piece(position_t* position, square_t from, square_t to);
void do_move(position_t* position, move_t move, undo_info_t* undo);
void undo_move(position_t* position, move_t move, undo_info_t* undo);
void do_nullmove(position_t* pos, undo_info_t* undo);
void undo_nullmove(position_t* pos, undo_info_t* undo);

// move_generation.c
int generate_legal_moves(position_t* pos, move_t* moves);
int generate_pseudo_moves(const position_t* position, move_t* move_list);
int generate_pseudo_tactical_moves(const position_t* pos, move_t* moves);
int generate_pseudo_quiet_moves(const position_t* pos, move_t* moves);
int generate_evasions(const position_t* pos, move_t* moves);
int generate_pseudo_checks(const position_t* pos, move_t* moves);
int generate_quiescence_moves(const position_t* pos,
        move_t* moves,
        bool generate_checks);

// move_selection.c
void init_move_selector(move_selector_t* sel,
        position_t* pos,
        generation_t gen_type,
        search_node_t* search_node,
        move_t hash_move,
        int depth,
        int ply);
bool has_single_reply(move_selector_t* sel);
bool should_try_prune(move_selector_t* sel, move_t move);
float lmr_reduction(move_selector_t* sel, move_t move);
move_t select_move(move_selector_t* sel);
bool defer_move(move_selector_t* sel, move_t move);
void init_pv_cache(const int max_bytes);
void clear_pv_cache(void);
void add_pv_move(move_selector_t* sel, move_t move, int64_t nodes);
void commit_pv_moves(move_selector_t* sel);
void print_pv_cache_stats(void);

// output.c
void print_coord_move(move_t move);
void print_coord_square(square_t square);
int print_coord_move_list(const move_t* move);
void print_search_stats(const search_data_t* search_data);
void print_board(const position_t* pos, bool uci_prefix);
void print_multipv(search_data_t* data);

// perft.c
void perft_testsuite(char* filename);
uint64_t perft(position_t* position, int depth, bool divide);

// position.c
char* set_position(position_t* pos, const char* fen);
void copy_position(position_t* dst, const position_t* src);
bool is_move_legal(position_t* pos, const move_t move);
bool is_plausible_move_legal(position_t* pos, move_t move);
bool is_pseudo_move_legal(position_t* pos, move_t move);
bool is_check(const position_t* pos);
bool is_repetition(const position_t* pos);

// search.c
void init_search_data(search_data_t* data);
void init_root_move(root_move_t* root_move, move_t move);
bool should_stop_searching(search_data_t* data);
void store_root_node_count(move_t move, uint64_t nodes);
void deepening_search(search_data_t* search_data, bool ponder);

// static_exchange_eval.c
int static_exchange_eval(const position_t* pos, move_t move);

// timer.c
void init_timer(milli_timer_t* timer);
void start_timer(milli_timer_t* timer);
int stop_timer(milli_timer_t* timer);
int elapsed_time(milli_timer_t* timer);

// trans_table.c
void init_transposition_table(const size_t max_bytes);
void clear_transposition_table(void);
void increment_transposition_age(void);
transposition_entry_t* get_transposition(position_t* pos);
void put_transposition(position_t* pos,
        move_t move,
        int depth,
        int score,
        score_type_t score_type,
        bool mate_threat);
void put_transposition_line(position_t* pos,
        move_t* moves,
        int depth,
        int score,
        score_type_t score_type);
void print_transposition_stats(void);

// uci.c
void uci_read_stream(FILE* stream);
void uci_check_for_command(void);
void uci_wait_for_command(void);

// uci_option.c
void init_uci_options(void);
void set_uci_option(char* command);
int get_option_int(const char* name);
bool get_option_bool(const char* name);
char* get_option_string(const char* name);
void print_uci_options(void);


#ifdef __cplusplus
} // extern "C"
#endif
#endif // DAYDREAMER_H

