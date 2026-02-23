#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include "threadpool.h"

#define NUM_WORKERS 4
#define NUM_PRODUCERS 4
#define TASKS_PER_PRODUCER 25000 // 100,000 tasks total
#define QUEUE_CAPACITY 1024

// Atomic counter to verify Exactly-Once execution
atomic_int total_executed = 0;

// The task payload
void stress_task(void *arg)
{
    atomic_fetch_add(&total_executed, 1);

    // A tiny volatile loop to simulate work and force the OS to interleave threads.
    // Without this, one thread might process everything before others wake up.
    for (volatile int i = 0; i < 50; i++)
        ;
}

// Producer thread logic
void *producer_thread(void *arg)
{
    global_queue_t *q = (global_queue_t *)arg;

    for (int i = 0; i < TASKS_PER_PRODUCER; i++)
    {
        // Primitive Backpressure: If queue is full, yield the CPU and try again.
        while (!submit_task(q, stress_task, NULL))
        {
            sched_yield();
        }
    }
    return NULL;
}

int main()
{
    global_queue_t q;
    pthread_t workers[NUM_WORKERS];
    pthread_t producers[NUM_PRODUCERS];

    printf("Initializing Queue (Capacity: %d)...\n", QUEUE_CAPACITY);
    queue_init(&q, QUEUE_CAPACITY);

    printf("Spawning %d Workers...\n", NUM_WORKERS);
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_create(&workers[i], NULL, worker_thread, &q);
    }

    printf("Spawning %d Producers to submit %d tasks total...\n",
           NUM_PRODUCERS, NUM_PRODUCERS * TASKS_PER_PRODUCER);
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_create(&producers[i], NULL, producer_thread, &q);
    }

    // Wait for all producers to finish submitting
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }
    printf("All tasks submitted. Initiating Shutdown...\n");

    // Initiate shutdown and wait for workers to drain the queue and exit
    queue_initiate_shutdown(&q);

    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_join(workers[i], NULL);
    }

    queue_destroy(&q);

    // --- The Moment of Truth ---
    int final_count = atomic_load(&total_executed);
    printf("Expected Tasks Executed: %d\n", NUM_PRODUCERS * TASKS_PER_PRODUCER);
    printf("Actual Tasks Executed:   %d\n", final_count);

    if (final_count == (NUM_PRODUCERS * TASKS_PER_PRODUCER))
    {
        printf("SUCCESS: No tasks dropped or duplicated. Invariant 1 held.\n");
    }
    else
    {
        printf("FAILURE: Race condition detected!\n");
    }

    return 0;
}