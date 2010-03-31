
#ifndef TRANS_TABLE_H
#define TRANS_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

// TODO: shrink this structure.
// TODO: track mate threats and whether null moves should be attempted
typedef struct {
    hashkey_t key;
    move_t move;
    int16_t score;
    uint8_t depth;
    uint8_t info;
} transposition_entry_t;

#define get_entry_flags(entry)      ((entry)->info & 0xf)
#define set_entry_flags(entry,flags)    \
    ((entry)->info = (get_entry_age(entry) << 4) + (flags))

#ifdef __cplusplus
} // extern "C"
#endif
#endif // TRANS_TABLE_H
