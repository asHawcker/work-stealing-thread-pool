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
        // TODO: Read the task at q->tasks[q->head] into a local variable (e.g., current_task)
        // TODO: Advance q->head. (Hint: Use modulo arithmetic to wrap around: (head + 1) % capacity)
        // TODO: Decrement q->count

        task_t current_task; /* assign this from your array */

        // CRITICAL: Release the lock BEFORE executing the function.
        // If you hold the lock during execution, you serialize your entire program!
        pthread_mutex_unlock(&q->lock);

        // Execute the task
        if (current_task.function)
        {
            current_task.function(current_task.arg);
        }
    }
    return NULL;
}