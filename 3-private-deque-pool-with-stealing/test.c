#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include "threadpool.h"

#define NUM_WORKERS 4
#define NUM_PRODUCERS 4
#define TASKS_PER_PRODUCER 100000 // 400,000 tasks total
#define QUEUE_CAPACITY 2048

// Atomic counter mapped directly to main memory to prevent caching discrepancies
atomic_int total_executed = 0;

#include <sys/time.h>

void stress_task(void *arg)
{
    // atomic_fetch_add returns the OLD value, so we add 1 to get the current count
    int current = atomic_fetch_add(&total_executed, 1) + 1;

    if (current % 50000 == 0)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);

        // Print the seconds and microseconds timestamp
        printf("[%ld.%06ld] Progress: %d / %d tasks executed...\n",
               (long)tv.tv_sec, (long)tv.tv_usec, current, NUM_PRODUCERS * TASKS_PER_PRODUCER);
    }

    // A tiny loop to simulate computational work
    for (volatile int i = 0; i < 20; i++)
        ;
}

void *producer_thread(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;
    for (int i = 0; i < TASKS_PER_PRODUCER; i++)
    {
        // Our submit_task now handles backpressure and yields internally
        submit_task(pool, stress_task, NULL);
    }
    return NULL;
}

int main()
{
    thread_pool_t pool;
    pthread_t producers[NUM_PRODUCERS];
    pthread_t workers[NUM_WORKERS];
    worker_args_t args[NUM_WORKERS];

    printf("--- Initializing Work-Stealing Pool ---\n");
    pool_init(&pool, NUM_WORKERS, QUEUE_CAPACITY);

    printf("--- Spawning %d Workers ---\n", NUM_WORKERS);
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        args[i].pool = &pool;
        args[i].worker_id = i;
        pthread_create(&workers[i], NULL, worker_thread, &args[i]);
    }

    printf("--- Spawning %d Producers (%d Tasks) ---\n", NUM_PRODUCERS, NUM_PRODUCERS * TASKS_PER_PRODUCER);
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_create(&producers[i], NULL, producer_thread, &pool);
    }

    // Wait for producers to finish submitting
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }
    printf("--- All Tasks Submitted. Initiating Drain & Shutdown ---\n");

    // Initiate Graceful Shutdown
    pthread_mutex_lock(&pool.sleep_lock);
    pool.shutdown = true;
    pthread_cond_broadcast(&pool.sleep_notify);
    pthread_mutex_unlock(&pool.sleep_lock);

    // Wait for workers to drain the remaining queues and exit
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_join(workers[i], NULL);
    }

    pool_destroy(&pool);

    int final_count = atomic_load(&total_executed);
    printf("Expected: %d\n", NUM_PRODUCERS * TASKS_PER_PRODUCER);
    printf("Actual:   %d\n", final_count);

    if (final_count == (NUM_PRODUCERS * TASKS_PER_PRODUCER))
    {
        printf("\nSUCCESS: Exactly-Once Execution Invariant Maintained!\n");
    }
    else
    {
        printf("\nFAILURE: Race Condition or Dropped Task Detected!\n");
    }

    return 0;
}