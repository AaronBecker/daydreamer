
#ifndef DEBUG_H
#define DEBUG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdio.h>
#if 0 // non-aborting assert
#undef assert
#undef __assert
#define assert(e)  \
        ((void) ((e) ? 0 : __assert (#e, __FILE__, __LINE__)))
#define __assert(e, file, line) \
        ((void)printf ("%s:%u: failed assertion `%s'\n", file, line, e))
#endif

#define warn(cond, msg)  \
        ((void) ((cond) ? 0 : __warn (#cond, __FILE__, __LINE__, msg)))
#define __warn(cond, file, line, msg) \
        ((void)printf ("%s:%u: warning: %s `%s'\n", file, line, msg, cond))

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

#ifdef __cplusplus
} // extern "C"
#endif
#endif // DEBUG_H
