
#ifndef TRANS_TABLE_H
#define TRANS_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

// TODO: shrink this structure.
typedef struct {
    hashkey_t key;
    move_t move;
    uint16_t depth;
    int32_t score;
    uint8_t age;
    score_type_t score_type;
} transposition_entry_t;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // TRANS_TABLE_H
