
#ifndef EVAL_MATERIAL_H
#define EVAL_MATERIAL_H
#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t endgame_flags_t;

typedef enum {
    EG_NONE,
    EG_WIN,
    EG_DRAW,

    EG_KQKQ,
    EG_KQKP,

    EG_KRKR,
    EG_KRKB,
    EG_KRKN,
    EG_KRKP,
    EG_KRPKR,
    EG_KRPPKRP,

    EG_KBBKN,
    EG_KBNK,
    EG_KBPKB,
    EG_KBPKN,
    EG_KPKB,
    EG_KBPPKB,

    EG_KNPK,
    EG_KBPK,
    
    EG_KPK,

    EG_LAST
} endgame_type_t;


typedef struct {
    hashkey_t key;
    endgame_flags_t eg_flags;
    endgame_type_t eg_type;
    int phase;
    int scale[2];
    score_t score;
    color_t strong_side;
} material_data_t;


#ifdef __cplusplus
}
#endif
#endif
