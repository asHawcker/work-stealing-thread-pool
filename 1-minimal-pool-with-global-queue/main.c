#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "threadpool.h"

#define NUM_THREADS 4
#define NUM_TASKS 15

void print_task(void *arg)
{
    long task_id = (long)arg;
    printf("Thread %lu executing Task %ld\n", (unsigned long)pthread_self(), task_id);
    usleep(10000); // sleep to simulate work
}

int main()
{
    global_queue_t q;
    pthread_t workers[NUM_THREADS];

    printf("Initializing Queue\n");
    queue_init(&q, 100);

    printf("Spawning Workers\n");
    for (int i = 0; i < NUM_THREADS; i++)
    {
        if (pthread_create(&workers[i], NULL, worker_thread, &q) != 0)
        {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }

    printf("Submitting Tasks\n");
    for (long i = 1; i <= NUM_TASKS; i++)
    {
        if (!submit_task(&q, print_task, (void *)i))
        {
            printf("Failed to submit task %ld (Queue full)\n", i);
        }
    }

    printf("Initiating Shutdown\n");
    queue_initiate_shutdown(&q);

    printf("Joining Threads\n");
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(workers[i], NULL);
    }

    printf("Destroying Queue\n");
    queue_destroy(&q);

    printf("Shutdown complete. All memory freed. Exiting cleanly.\n");
    return 0;
}