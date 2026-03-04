# Work-Stealing Task Scheduler in C

A high-performance, multithreaded task scheduler implementing a decentralized, work-stealing architecture in C using POSIX threads (`pthreads`).

# Architectural Overview

This scheduler moves beyond naive centralized thread pools, which suffer from severe lock contention on a single global queue at scale. Instead, it adopts a distributed "M:N" scheduling philosophy, mapping application tasks (N) onto fixed OS worker threads (M).

## Key Architectural Pillars

- **Distributed Deques**: Every worker thread owns a private Double-Ended Queue (Deque), completely isolating fast-path push/pop operations.

- **Work-Stealing**: When a worker's local deque is empty, it acts as a thief, randomly selecting peer queues to steal work from.

- **Hardware-Aware Data Design**: Cache-line alignment (`alignas(64)`) is used on per-thread data structures to physically prevent False Sharing, which would otherwise devastate performance via cache coherency traffic.

- **LIFO/FIFO Duality**:
  - **Owner (LIFO)**: Pushes and pops from the `bottom`. This stack-like behavior ensures superior Temporal Locality, keeping data hot in the physical core's L1 cache.

  - **Thief (FIFO)**: Steals older tasks from the `top`. This minimizes collision with the owner and often captures larger "parent" tasks, balancing load more effectively.

- **Graceful Shutdown & Backpressure**: Enforces global liveness invariants during shutdown (draining all peers before exiting) and utilizes randomized retries with `sched_yield`for producer backpressure.

## Architecture Diagrams

1. **Global Component Overview**
   This diagram shows the system flow from task submission by Producers to decentralized consumption and stealing by Workers.

```mermaid
flowchart TD
    %% 1. Top Level
    subgraph Producers ["Task Submission (Main/App Threads)"]
        direction LR
        P1["Producer 1"]
        P2["Producer 2"]
    end

    %% 2. Middle Level
    subgraph Workers ["Worker Thread Pool (Fixed Size M)"]
        direction TB
        subgraph W0 ["Worker Core 0"]
            direction LR
            Thread0["Worker Thread 0"]
            Queue0["Local Deque 0 (Cache-Line Aligned)"]
        end
        subgraph W1 ["Worker Core 1"]
            direction LR
            Thread1["Worker Thread 1"]
            Queue1["Local Deque 1 (Cache-Line Aligned)"]
        end
        subgraph WN ["Worker Core N"]
            direction LR
            ThreadN["Worker Thread N"]
            QueueN["Local Deque N (Cache-Line Aligned)"]
        end
    end

    %% 3. Bottom Level
    subgraph PoolState ["Global Pool State"]
        direction LR
        SD["Shutdown Flag (bool)"]
        SL["Sleep Mutex"]
        SC["Sleep Cond_Var"]
    end

    %% Interactions

    %% Uncrossed for cleaner routing
    P1 --->|"1. submit_task()"| Queue0
    P2 --->|"1. submit_task()"| Queue1

    Producers -.->|"2. Wake Sleeper"| SC

    Thread0 <==>|"Fast Path: Push/Pop (LIFO)"| Queue0
    Thread1 <==>|"Fast Path: Push/Pop (LIFO)"| Queue1

    %% Stealing paths (The cross is unavoidable, but pushing it lower helps)
    Thread1 -.->|"Starving: steal_task() (FIFO)"| Queue0
    Thread0 -.->|"Starving: steal_task() (FIFO)"| Queue1

    Workers -.->|"System Empty (Sleep/Wait)"| SC
```

<br><br> 2. **Deque Micro-Architecture & Concurrency Model**
This diagram details the interaction between the local Owner and a remote Thief on a single Bounded Circular Buffer Deque.

```mermaid
flowchart LR
    subgraph Threads ["Concurrent Access"]
        Thief["Remote Thread (Thief)"]
        Owner["Local Thread (Owner)"]
    end

    subgraph Deque ["worker_queue_t (alignas(64))"]
        direction TB
        subgraph Sync ["Synchronization"]
            Lock["pthread_mutex_t"]
        end

        subgraph Storage ["Circular Buffer Task Array"]
            T0["Task 0"]
            T1["Task 1"]
            T2["Empty"]
            T3["Empty"]
            TN["Task N-1"]
        end

        subgraph Pointers ["Indices"]
            TopIdx["top: Index 0"]
            BottomIdx["bottom: Index 2"]
        end
    end

    %% Interactions
    Owner ==>|"1. Lock"| Lock
    Owner ==>|"2a. Push/Pop (LIFO/Bottom)"| BottomIdx
    Owner ==>|"3. Map: bottom % cap"| Storage
    Owner ==>|"4. Unlock"| Lock

    BottomIdx -.->|"bottom++ (Push)"| Storage
    BottomIdx -.->|"bottom-- (Pop)"| Storage

    Thief -->|"1. Lock (Victim Lock)"| Lock
    Thief -->|"2a. Steal (FIFO/Top)"| TopIdx
    Thief -->|"3. Map: top % cap"| Storage
    Thief -->|"4. Unlock (Victim Lock)"| Lock

    TopIdx -.->|"top++ (Steal)"| Storage
```

# Build and Test

## Prerequisites (basically none)

- A C compiler supporting `C11`.
- Linux/POSIX environment (`pthreads`).

## Compilation

Build the core library and the example program:

```c
// Basic build with optimizations
gcc -O2 -o pool main.c threadpool.c -lpthread

// Or separate compilation (cleaner)
gcc -O2 -c threadpool.h -o threadpool.o
gcc -O2 main.c threadpool.o -o pool -lpthread
```

## Verification & Stress Testing

We validate the scheduler invariants using a stress test with multiple producers.

```c
# Compile the stress test
gcc -O2 -o stress test.c threadpool.c -lpthread

# Run the stress test (400,000 tasks total)
./stress
```
