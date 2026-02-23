## Tests

1. Test for Race Conditions (ThreadSanitizer)
   This will severely slow down execution but will detect if two threads access q->head or q->count simultaneously without holding the mutex.

```bash
gcc -g -O2 -fsanitize=thread -o test_stress test.c -lpthread
setarch $(uname -m) -R ./test_stress
```

- Acceptable output

  ```c
  Initializing Queue (Capacity: 1024)...
  Spawning 4 Workers...
  Spawning 4 Producers to submit 100000 tasks total...
  All tasks submitted. Initiating Shutdown...
  Expected Tasks Executed: 100000
  Actual Tasks Executed:   100000
  SUCCESS: No tasks dropped or duplicated. Invariant 1 held.
  ```

- TSan(Thread Sanitizer) relies on heavily instrumenting the virtual memory space and expects a very specific memory layout to track thread accesses. However, modern Linux uses aggressive Address Space Layout Randomization (ASLR) for security, which places memory mappings in areas TSan doesn't expect, causing it to panic and crash during its own initialization before the `main()` even runs.
  To workaround this, we can temporarily disable ASLR for that specific execution using the `setarch` command for the test and we pass the `-R` flag to disable the randomization, and run the program.

2. Test for Memory Leaks (AddressSanitizer)
   Ensures queue_destroy properly frees our contiguous task array.

```bash
gcc -g -O2 -fsanitize=address -o test_stress test.c -lpthread
./test_stress
```

    - Expected output

    ```c
    Initializing Queue (Capacity: 1024)...
    Spawning 4 Workers...
    Spawning 4 Producers to submit 100000 tasks total...
    All tasks submitted. Initiating Shutdown...
    Expected Tasks Executed: 100000
    Actual Tasks Executed:   100000
    SUCCESS: No tasks dropped or duplicated. Invariant 1 held.
    ```
