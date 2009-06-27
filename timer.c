
#include "grasshopper.h"

/*
 * Start up a timer with a new clock.
 */
void start_timer(timer_t* timer)
{
    timer->elapsed_millis = 0;
    gettimeofday(&timer->tv_start, NULL);
}

/*
 * Stop a running timer, returning the elapsed milliseconds since it was
 * last started. The timer can be resumed using resume_timer.
 */
int stop_timer(timer_t* timer)
{
    struct timeval tv_end;
    gettimeofday(&tv_end, NULL);
    int elapsed_millis = (tv_end.tv_sec - timer->tv_start.tv_sec)*1000 +
        (tv_end.tv_usec - timer->tv_start.tv_usec)/1000;
    timer->elapsed_millis += elapsed_millis;
    return elapsed_millis;
}

/*
 * Restart a timer that's been stopped without resetting its internal clock.
 */
void resume_timer(timer_t* timer)
{
    gettimeofday(&timer->tv_start, NULL);
}

/*
 * Reset the internal clock of a timer.
 */
void reset_timer(timer_t* timer)
{
    timer->elapsed_millis = 0;
}

/*
 * Get the number of milliseconds elapsed over all the intervals during which
 * this timer has been running since it was last reset.
 */
int elapsed_time(timer_t* timer)
{
    return timer->elapsed_millis;
}

