#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>

void pool_init(thread_pool_t *pool, int num_workers, int queue_capacity)
{
    pool->num_workers = num_workers;
    pool->shutdown = false;

    pthread_mutex_init(&pool->sleep_lock, NULL);
    pthread_cond_init(&pool->sleep_notify, NULL);

    int err = posix_memalign((void **)&pool->queues, 64, num_workers * sizeof(worker_queue_t));
    if (err != 0)
    {
        perror("Failed to allocate aligned memory for queues");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_workers; i++)
    {
        pool->queues[i].capacity = queue_capacity;
        pool->queues[i].top = 0;
        pool->queues[i].bottom = 0;
        pthread_mutex_init(&pool->queues[i].lock, NULL);

        pool->queues[i].tasks = (task_t *)malloc(queue_capacity * sizeof(task_t));
        if (!pool->queues[i].tasks)
        {
            perror("Failed to allocate task array");
            exit(EXIT_FAILURE);
        }
    }
}

bool submit_task(thread_pool_t *pool, void (*func)(void *), void *arg)
{
    int target_worker = rand() % pool->num_workers;
    worker_queue_t *target_queue = &pool->queues[target_worker];

    task_t new_task;
    new_task.function = func;
    new_task.arg = arg;

    pthread_mutex_lock(&target_queue->lock);
    if (target_queue->bottom >= target_queue->capacity)
    {
        pthread_mutex_unlock(&target_queue->lock);
        return false;
    }
    target_queue->tasks[target_queue->bottom] = new_task;
    target_queue->bottom++;
    pthread_mutex_unlock(&target_queue->lock);

    pthread_mutex_lock(&pool->sleep_lock);
    pthread_cond_signal(&pool->sleep_notify);
    pthread_mutex_unlock(&pool->sleep_lock);

    return true;
}

void *worker_thread(void *arg)
{
    worker_args_t *args = (worker_args_t *)arg;
    thread_pool_t *global_pool = args->pool;
    int my_id = args->worker_id;

    worker_queue_t *my_queue = &global_pool->queues[my_id];

    while (1)
    {
        task_t current_task;
        bool has_work = false;

        pthread_mutex_lock(&my_queue->lock);
        if (my_queue->bottom > my_queue->top)
        {
            my_queue->bottom--;
            current_task = my_queue->tasks[my_queue->bottom];
            has_work = true;
        }
        pthread_mutex_unlock(&my_queue->lock);

        if (has_work)
        {
            if (current_task.function)
            {
                current_task.function(current_task.arg);
            }
            continue;
        }

        pthread_mutex_lock(&global_pool->sleep_lock);
        if (global_pool->shutdown)
        {
            pthread_mutex_unlock(&global_pool->sleep_lock);
            break;
        }
        pthread_cond_wait(&global_pool->sleep_notify, &global_pool->sleep_lock);
        pthread_mutex_unlock(&global_pool->sleep_lock);
    }
    return NULL;
}

void pool_destroy(thread_pool_t *pool)
{
    for (int i = 0; i < pool->num_workers; i++)
    {
        free(pool->queues[i].tasks);
        pthread_mutex_destroy(&pool->queues[i].lock);
    }
    free(pool->queues);
    pthread_mutex_destroy(&pool->sleep_lock);
    pthread_cond_destroy(&pool->sleep_notify);
}