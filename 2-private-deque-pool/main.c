#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "threadpool.h"

#define NUM_WORKERS 4
#define QUEUE_CAPACITY 1024
#define NUM_TASKS 20

// A Dummy Task
void print_task(void *arg)
{
    long task_id = (long)arg;
    printf("[Thread %lu] Executing Task %ld\n", (unsigned long)pthread_self(), task_id);
    usleep(50000); // Sleep 50ms to simulate work
}

int main()
{
    srand(time(NULL));

    thread_pool_t pool;
    pthread_t threads[NUM_WORKERS];
    worker_args_t thread_args[NUM_WORKERS];

    printf("--- Initializing Phase 3B Pool ---\n");
    pool_init(&pool, NUM_WORKERS, QUEUE_CAPACITY);

    printf("--- Spawning Workers ---\n");
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        thread_args[i].pool = &pool;
        thread_args[i].worker_id = i;
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]) != 0)
        {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }

    printf("--- Submitting %d Tasks Randomly ---\n", NUM_TASKS);
    for (long i = 1; i <= NUM_TASKS; i++)
    {
        submit_task(&pool, print_task, (void *)i);
    }

    sleep(1);

    printf("--- Initiating Shutdown ---\n");
    pthread_mutex_lock(&pool.sleep_lock);
    pool.shutdown = true;
    pthread_cond_broadcast(&pool.sleep_notify);
    pthread_mutex_unlock(&pool.sleep_lock);

    printf("--- Joining Threads ---\n");
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    printf("--- Teardown ---\n");
    pool_destroy(&pool);

    printf("Clean exit.\n");
    return 0;
}