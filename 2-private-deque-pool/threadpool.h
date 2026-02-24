#include <pthread.h>
#include <stdbool.h>
#include <stdalign.h>

typedef struct
{
    void (*function)(void *arg);
    void *arg;
} task_t;

typedef struct
{
    task_t *tasks;
    int capacity;
    int top;
    int bottom;
    pthread_mutex_t lock;
} worker_queue_t __attribute__((aligned(64)));

typedef struct
{
    worker_queue_t *queues;
    int num_workers;

    bool shutdown;

    pthread_mutex_t sleep_lock;
    pthread_cond_t sleep_notify;
} thread_pool_t;

void local_push(worker_queue_t *queue, task_t task)
{
    pthread_mutex_lock(&queue->lock);
    if (queue->bottom < queue->capacity)
    {
        queue->tasks[queue->bottom] = task;
        queue->bottom++;
    }
    pthread_mutex_unlock(&queue->lock);
}

bool local_pop(worker_queue_t *queue, task_t *out_task)
{
    pthread_mutex_lock(&queue->lock);

    if (queue->bottom > queue->top)
    {
        queue->bottom--;
        *out_task = queue->tasks[queue->bottom];

        pthread_mutex_unlock(&queue->lock);
        return true;
    }
    else
    {
        pthread_mutex_unlock(&queue->lock);
        return false;
    }
}