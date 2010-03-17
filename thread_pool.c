
#include "daydreamer.h"
#include <sched.h>

static void* worker_loop(void *arg);

/*
 * Initialize a new thread pool. Note that storage for the worker threads'
 * per-thread info and argument data must be allocated by the caller, to
 * facilitate thread launching without any memory allocation or copying.
 */
void init_thread_pool(thread_pool_t* pool,
        void* arg_storage,
        thread_info_t* info_storage,
        int arg_size,
        int max_threads)
{
    pool->args = arg_storage;
    pool->info = info_storage;
    pool->num_threads = max_threads;
    pool->arg_size = arg_size;
    pool->idle_threads = 0;
    pool->quit = false;
    pthread_mutex_init(&pool->pool_mutex, NULL);
    pthread_attr_init(&pool->thread_attrs);
    pthread_attr_setdetachstate(&pool->thread_attrs, PTHREAD_CREATE_DETACHED);
    for (int i = 0; i < max_threads; ++i) {
        pool->info[i].idle = false;
        pthread_cond_init(&pool->info[i].cv, NULL);
        pthread_t pth;
	if (pthread_create(&pth,
                    &pool->thread_attrs,
                    worker_loop,
                    pool)) {
            perror("Thread pool creation failed.\n");
        }
        while (!pool->info[i].idle) sched_yield();
    }
}

/*
 * Tell all the worker threads to quit.
 */
void destroy_thread_pool(thread_pool_t* pool)
{
    pool->quit = true;
    while (pool->idle_threads != pool->num_threads) sched_yield();
    pthread_mutex_destroy(&pool->pool_mutex);
    pthread_attr_destroy(&pool->thread_attrs);
    for (int i = 0; i < pool->num_threads; ++i) {
        pthread_cond_destroy(&pool->info[i].cv);
    }
}

/*
 * Find an open slot in the allocated arrays of argument and thread
 * info. The caller then fills this slot on its own, and calls run_thread
 * on the appropriate slot.
 */
bool get_slot(thread_pool_t* pool, int* slot, void** arg_addr)
{
    // FIXME: this explicitly depends on having only one controlling thread.
    //        This is ok for now, but once we have SMP search it has to change.
    if (!pool->idle_threads) {
        return false;
    }
    pthread_mutex_lock(&pool->pool_mutex);
    int i;
    for (i=0; i < pool->num_threads && !pool->info[i].idle; ++i) {}
    assert(i != pool->num_threads && pool->info[i].idle);
    pthread_mutex_unlock(&pool->pool_mutex);
    *slot = i;
    *arg_addr = pool->args + i*pool->arg_size;
    assert(i != pool->num_threads && pool->info[i].idle);
    return true;
}

/*
 * Launch a worker thread using the argument and thread info
 * in the given slot.
 */
bool run_thread(thread_pool_t* pool, task_fn_t task, int slot)
{
    pthread_mutex_lock(&pool->pool_mutex);
    assert(pool->info[slot].idle);
    pool->info[slot].idle = false;
    pool->info[slot].task = task;
    pool->idle_threads--;
    pthread_cond_signal(&pool->info[slot].cv);
    pthread_mutex_unlock(&pool->pool_mutex);
    return true;
}

/*
 * The idle loop for all worker threads. We use condition variables
 * to wait on work instead of polling to reduce overhead.
 */
static void* worker_loop(void *arg)
{
    thread_pool_t* pool = (thread_pool_t*)arg;
    int thread_index = pool->idle_threads;
    task_fn_t work_fn = NULL;

    while (true) {
        // Update pool data and go to sleep.
        pthread_mutex_lock(&pool->pool_mutex);
        pool->idle_threads++;
        pool->info[thread_index].idle = true;
        if (pthread_cond_wait(&pool->info[thread_index].cv,
                    &pool->pool_mutex)) perror("Error waiting on CV");

        // We've been woken back up, there must be something for us to do.
        if (pool->quit) break;

        // Execute the desired background task.
        work_fn = pool->info[thread_index].task;
        arg = pool->args + thread_index*pool->arg_size;
        work_fn(arg);
        if (pool->quit) break;
    }
    return NULL;
}

