
#ifndef DEBUG_H
#define DEBUG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>

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
