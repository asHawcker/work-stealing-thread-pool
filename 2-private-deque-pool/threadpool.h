#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdbool.h>

// Task representation
typedef struct
{
    void (*function)(void *arg);
    void *arg;
} task_t;

// Per-thread Deque (Aligned to prevent False Sharing)
typedef struct __attribute__((aligned(64)))
{
    task_t *tasks;
    int capacity;
    int top;
    int bottom;
    pthread_mutex_t lock;
} worker_queue_t;

// Global Pool State
typedef struct
{
    worker_queue_t *queues;
    int num_workers;
    bool shutdown;

    pthread_mutex_t sleep_lock;
    pthread_cond_t sleep_notify;
} thread_pool_t;

// Struct to pass arguments to the worker thread
typedef struct
{
    thread_pool_t *pool;
    int worker_id;
} worker_args_t;

// Core API
void pool_init(thread_pool_t *pool, int num_workers, int queue_capacity);
bool submit_task(thread_pool_t *pool, void (*func)(void *), void *arg);
void *worker_thread(void *arg);
void pool_destroy(thread_pool_t *pool);

#endif // THREADPOOL_H