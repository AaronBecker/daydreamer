
#ifndef DEBUG_H
#define DEBUG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdio.h>
#if 0  // non-aborting, trace-logging assert
#include <execinfo.h>
#undef assert
#define assert(e)   do { \
    if (e) break; \
    FILE* log; \
    log = fopen("daydreamer.log", "a"); \
    fprintf(log, "%s:%u: failed assertion `%s'\n", __FILE__, __LINE__, #e); \
    printf("%s:%u: failed assertion `%s'\n", __FILE__, __LINE__, #e); \
    void* callstack[128]; \
    int frames = backtrace(callstack, 128); \
    char** strs = backtrace_symbols(callstack, frames); \
    for (int i = 0; i < frames; ++i) { \
        fprintf(log, "%s\n", strs[i]); \
    } \
    free(strs); \
    fclose(log); \
} while (0)
#endif

#define warn(msg)   do { \
    FILE* log; \
    log = fopen("daydreamer.log", "a"); \
    printf("%s:%u: %s\n", __FILE__, __LINE__, msg); \
    fprintf(log, "%s:%u: %s\n", __FILE__, __LINE__, msg); \
    fclose(log); \
} while (0)

#define log(msg)    do { \
    FILE* log; \
    log = fopen("daydreamer.log", "a"); \
    fprintf(log, "%s\n", msg); \
    fclose(log); \
} while (0)    

void _check_board_validity(const position_t* pos);
void _check_move_validity(const position_t* pos, move_t move);
void _check_pseudo_move_legality(position_t* pos, move_t move);
void _check_position_hash(const position_t* pos);
void _check_line(position_t* pos, move_t* line);

#ifdef OMIT_CHECKS
#define check_board_validity(x)                 ((void)0)
#define check_move_validity(x,y)                ((void)0,(void)0)
#define check_pseudo_move_legality(x,y)         ((void)0)
#define check_position_hash(x)                  ((void)0)
#define check_line(x,y)                         ((void)0)
#else
#define check_board_validity(x)                 _check_board_validity(x)
#define check_move_validity(x,y)                _check_move_validity(x,y)
#define check_pseudo_move_legality(x,y)         _check_pseudo_move_legality(x,y)
#define check_position_hash(x)                  _check_position_hash(x)
#define check_line(x,y)                         _check_line(x,y)
#endif

#ifdef __cplusplus
} // extern "C"
#endif
#endif // DEBUG_H
