
#ifndef EVAL_H
#define EVAL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef UFO_EVAL
    #define PAWN_VAL      100
    #define KNIGHT_VAL    320
    #define BISHOP_VAL    330
    #define ROOK_VAL      500
    #define QUEEN_VAL     900
    #define KING_VAL      20000
#else
    #define PAWN_VAL         100
    #define KNIGHT_VAL       325
    #define BISHOP_VAL       325
    #define ROOK_VAL         500
    #define QUEEN_VAL        975
    #define KING_VAL         20000
    #define EG_PAWN_VAL      100
    #define EG_KNIGHT_VAL    325
    #define EG_BISHOP_VAL    325
    #define EG_ROOK_VAL      500
    #define EG_QUEEN_VAL     975
    #define EG_KING_VAL      20000
#endif
#define WON_ENDGAME         (2*QUEEN_VAL)

extern int piece_square_values[BK+1][0x80];
extern int endgame_piece_square_values[BK+1][0x80];
extern const int material_values[];
extern const int eg_material_values[];
#define material_value(piece)               material_values[piece]
#define eg_material_value(piece)            eg_material_values[piece]
#define piece_square_value(piece, square)   piece_square_values[piece][square]
#define endgame_piece_square_value(piece, square) \
    endgame_piece_square_values[piece][square]

typedef struct {
    int midgame;
    int endgame;
} score_t;

#include "pawn.h"
#include "position.h"
#include "eval_material.h"

typedef struct {
    pawn_data_t* pd;
    material_data_t* md;
} eval_data_t;

typedef void(*eg_scale_fn)(const position_t*, eval_data_t*, int scale[2]);
typedef int(*eg_score_fn)(const position_t*, eval_data_t*);
#define endgame_scale_function(md)   (eg_scale_fns[(md)->eg_type])

#ifdef __cplusplus
} // extern "C"
#endif
#endif // EVAL_H
