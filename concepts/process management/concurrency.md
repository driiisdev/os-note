# Concurrency

As soon as two threads share memory, the order in which their instructions interleave becomes
part of your program's behavior - and the OS scheduler, not you, chooses that order. Concurrency
is about writing code that is correct **for every possible interleaving**, using the tools the
OS and the CPU provide: locks, atomics, condition variables, and careful design.

## Concurrency vs parallelism

- **Concurrency** - multiple tasks *in progress* at the same time, possibly interleaved on one
  core. A structure.
- **Parallelism** - multiple tasks *executing* at the same instant, on different cores. A
  property of the hardware running it.

A single-core machine can be concurrent but never parallel. Races happen in both cases:
preemption can land between any two instructions.

## The race condition

```c
counter++;    // looks like one step
```

compiles to three:

```text
load  counter -> register
add   register, 1
store register -> counter
```

```text
 thread A                      thread B
 load  counter (= 7)
                               load  counter (= 7)
 add   -> 8
                               add   -> 8
 store counter = 8
                               store counter = 8      <- one increment is lost
```

A **race condition** is any bug where the result depends on timing. The code that touches the
shared state is the **critical section**, and the fix is to make it **mutually exclusive** or
**atomic**.

🖼️ **lost update:**
![lost update race](<../../assets/img/race-condition.png>)

## The toolbox

| Tool | What it gives you | When to use it |
| --- | --- | --- |
| **Mutex** (`pthread_mutex_t`) | one thread at a time in the critical section; waiters sleep | general-purpose protection of shared data |
| **Spinlock** | like a mutex, but waiters busy-loop instead of sleeping | very short critical sections, kernel code that can't sleep |
| **Read-write lock** | many readers *or* one writer | read-heavy shared data |
| **Semaphore** | a counter: allow up to N threads in | limiting concurrency (e.g. N connections), signalling |
| **Condition variable** | sleep until some condition is true, woken by another thread | producer/consumer queues, "wait until ready" |
| **Atomics** (`__atomic_*`, `std::atomic`, `AtomicLong`) | single operations that are indivisible in hardware - `fetch_add`, compare-and-swap | counters, flags, lock-free data structures |
| **Not sharing** | per-thread state, combined at the end; message passing | whenever you can - the fastest and safest option |

On Linux, `pthread_mutex` is built on the **futex** syscall: an uncontended lock/unlock is just
an atomic instruction in user space; the kernel is only involved when a thread actually has to
sleep or be woken.

## Deadlock

Two threads each hold one lock and wait for the other's - forever. A deadlock needs **all four**
Coffman conditions at once:

1. **Mutual exclusion** - the resource can only be held by one thread.
2. **Hold and wait** - a thread holds one resource while waiting for another.
3. **No preemption** - resources can't be taken away.
4. **Circular wait** - A waits for B, B waits for A (or a longer cycle).

Break any one and deadlock becomes impossible. The usual practical fix is a **global lock order**
(always take A before B), which removes circular wait. Others: `trylock` with back-off (breaks
hold-and-wait), timeouts, or a single coarse lock.

Related failure modes:

- **Livelock** - threads keep reacting to each other and retrying, but nobody makes progress.
- **Starvation** - a thread never gets the lock because others keep winning.
- **Priority inversion** - a high-priority thread waits for a lock held by a low-priority thread
  that never gets scheduled. NASA's Mars Pathfinder (1997) kept resetting until engineers enabled
  **priority inheritance** on the mutex remotely.

## Memory ordering (one paragraph)

CPUs and compilers reorder memory operations for speed. On one thread you never notice; across
threads, one core can observe another's writes in a surprising order. Locks and atomics include
**memory barriers** that prevent harmful reordering - which is why "just use a `volatile` flag"
is not a correct synchronization strategy in C/C++. (`volatile` stops the compiler from caching
a value in a register; it makes nothing atomic and orders nothing across cores.)

## Clarifications

- **Locks make code correct, not fast.** A contended lock serializes threads; the practice
  project's mutex version is ~65x slower than the (wrong) unsynchronized one.
- **The best lock is no lock** - partition the data, give each thread its own, or pass messages.
- **Single-threaded event loops still have concurrency** - between callbacks, the state can change
  under you (e.g. two requests interleaving across `await` points).
- **Races can hide for years.** They depend on timing, so tools like **ThreadSanitizer**
  (`-fsanitize=thread`) and Helgrind exist to find them deterministically.

## Practice project

[`practice/process-management/race-condition-lab`](../../practice/process-management/race-condition-lab) -
4 threads × 1M increments lose half to three-quarters of the updates without synchronization;
mutex, atomic and per-thread-counter fixes compared on time; a two-lock deadlock caught by a
watchdog, then fixed with a lock order; `make tsan` reports the race with stack traces.

## Related

- [Process vs thread](process-vs-thread.md) - shared memory is what makes this necessary.
- [Data section](<../anatomy of process/data-section.md>) and [the heap](<../anatomy of process/the-heap.md>) -
  the shared memory regions races happen in.
- [Reading and writing from and to memory](<../memory management/reading-n-writing-from-n-to-memory.md>) -
  cache coherence, the hardware side of atomics.
