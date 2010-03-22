
#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef GTB_WINDOWS
#include <pthread.h>

typedef void*(*task_fn_t)(void*);

typedef struct {
    uint8_t idle;
    task_fn_t task;
    pthread_cond_t cv;
    uint8_t _pad[CACHE_LINE_BYTES -
        ((sizeof(uint8_t) +
        sizeof(task_fn_t) +
        sizeof(pthread_cond_t)) % CACHE_LINE_BYTES)];
} thread_info_t;

typedef struct {
    void* args;
    thread_info_t* info;
    int arg_size;
    int num_threads;
    int idle_threads;
    bool quit;
    pthread_attr_t thread_attrs;
    pthread_mutex_t pool_mutex;
    pthread_cond_t work_cv;
} thread_pool_t;

#else

typedef void*(*task_fn_t)(void*);

typedef struct {
    int dummy;
} thread_info_t;

typedef struct {
    int dummy;
} thread_pool_t;

#endif

#ifdef __cplusplus
} // extern "C"
#endif
#endif // THREAD_POOL_H
