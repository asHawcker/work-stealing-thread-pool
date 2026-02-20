#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct
{
    void (*function)(void *arg);
    void *arg;
} task_t;

typedef struct
{
    task_t *tasks;
    int head;
    int tail;
    int count;
    int capacity;

    bool shutdown;

    pthread_mutex_t lock;
    pthread_cond_t notify;
} global_queue_t;

void queue_init(global_queue_t *q, int capacity)
{
    q->capacity = capacity;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->tasks = (task_t *)malloc(capacity * sizeof(task_t));
    q->shutdown = false;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->notify, NULL);
}

void *worker_thread(void *arg)
{
    global_queue_t *q = (global_queue_t *)arg;

    while (1)
    {
        pthread_mutex_lock(&q->lock);

        while (q->count == 0 && !q->shutdown)
        {
            pthread_cond_wait(&q->notify, &q->lock);
        }

        // check for shutdown AFTER waking up
        if (q->shutdown && q->count == 0)
        {
            pthread_mutex_unlock(&q->lock);
            break;
        }

        // Extract work from the circular buffer
        task_t current_task = q->tasks[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->count--;

        // CRITICAL: Release the lock BEFORE executing the function.
        pthread_mutex_unlock(&q->lock);

        // Execute the task
        if (current_task.function)
        {
            current_task.function(current_task.arg);
        }
    }
    return NULL;
}

bool submit_task(global_queue_t *q, void (*func)(void *), void *arg)
{
    pthread_mutex_lock(&q->lock);

    if (q->count == q->capacity)
    {
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    q->tasks[q->tail].function = func;
    q->tasks[q->tail].arg = arg;

    q->tail = (q->tail + 1) % q->capacity;

    q->count++;

    pthread_cond_signal(&q->notify);

    pthread_mutex_unlock(&q->lock);
    return true;
}