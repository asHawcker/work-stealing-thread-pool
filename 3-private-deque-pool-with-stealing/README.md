## Problems

1. False Sharing
   If we create an array of the per-thread queues:
   `worker_queue_t queues[NUM_WORKERS];`

   In C, arrays are contiguous in memory. If `sizeof(worker_queue_t)` is 32 bytes, then `queues[0]` and `queues[1]` will sit directly next to each other.

   Modern CPUs fetch memory from RAM into their L1/L2 caches in 64-byte chunks called Cache Lines.

   If Thread 0 is hammering `queues[0].bottom` and Thread 1 is hammering `queues[1].bottom`, and both structs happen to live on the same 64-byte cache line, the physical CPU cores will constantly invalidate each other's L1 caches. Even though they are technically accessing completely different memory addresses, the hardware would treat it as a collision. It's like we removed **lock contention**, but now we have accidentally created **cache contention**.

   To fix this, we pad our structs to align with the CPU cache lines using C11's `alignas(64)` (or compiler attributes) to force the allocator to place each `worker_queue_t` exactly 64 bytes apart.

2. Which thread's queue should I add task to?

   If the main thread locked every queue to check its bottom index just to find the least busy worker, it would completely serialize the system and create a bottleneck. So we allocate the task by generating a random index and blindly pushing the task to that specific worker's queue, we achieve _O(1)_ statistical load balancing with zero cross-thread coordination. This also goes well with the work stealing logic.
