
#include "daydreamer.h"
#include <sched.h>

static void* worker_thread(void *arg);

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
                    worker_thread,
                    pool)) {
            perror("Thread pool creation failed.\n");
        }
        while (!pool->info[i].idle) sched_yield();
    }
}

void destroy_thread_pool(thread_pool_t* pool)
{
    pool->quit = true;
    while (pool->idle_threads != pool->num_threads) sched_yield();
}

// Note: this explicitly depends on having only one controlling thread.
bool get_slot(thread_pool_t* pool, int* slot, void** arg_addr)
{
    int i;
    if (!pool->idle_threads) return false;
    pthread_mutex_lock(&pool->pool_mutex);
    for (i=0; i < pool->num_threads && !pool->info[i].idle; ++i) {}
    assert(i != pool->num_threads && pool->info[i].idle);
    pthread_mutex_unlock(&pool->pool_mutex);
    *slot = i;
    *arg_addr = pool->args + i*pool->arg_size;
    return true;
}

bool run_thread(thread_pool_t* pool, task_fn_t task, int slot)
{
    pool->info[slot].task = task;
    pthread_mutex_lock(&pool->pool_mutex);
    pthread_cond_signal(&pool->info[slot].cv);
    pthread_mutex_unlock(&pool->pool_mutex);
    return true;
}

static void* worker_thread(void *arg)
{
    thread_pool_t* pool = (thread_pool_t*)arg;
    int thread_index = pool->idle_threads;
    task_fn_t work_fn = NULL;

    while (true) {
        pthread_mutex_lock(&pool->pool_mutex);
        pool->idle_threads++;
        pool->info[thread_index].idle = true;
        pthread_cond_wait(&pool->info[thread_index].cv, &pool->pool_mutex);

        pool->idle_threads--;
        pool->info[thread_index].idle = false;
        if (pool->quit) break;

        pthread_mutex_unlock(&pool->pool_mutex);
        work_fn = pool->info[thread_index].task;
        arg = pool->args + thread_index*pool->arg_size;
        work_fn(arg);
        if (pool->quit) break;
    }
    return NULL;
}

