# Practice: Race Condition Lab

Concept note: [`concurrency.md`](<../../../concepts/process management/concurrency.md>)

## What this demonstrates

1. **A race condition**, live: four threads each add 1 to a shared counter a million times.
   The answer should be 4,000,000. Without synchronization it's wrong - and *differently*
   wrong on every run.
2. **Three fixes and their costs**: a **mutex**, an **atomic** instruction, and **not sharing**
   at all (each thread counts locally, then combines once).
3. **A deadlock**: two threads, two locks, taken in opposite orders - and the fix, a global
   lock order.

## Folder structure

```text
race-condition-lab/
  race.c       <- the shared counter: none | mutex | atomic | local
  deadlock.c   <- lock-order deadlock with a watchdog, and the ordered fix
  Makefile     <- `make`, `make demo`, `make tsan` (ThreadSanitizer)
```

## Build & run

```bash
make demo
./bin/race none      # run it a few times
make tsan            # the race reported by a tool, with stack traces
```

## What you'll actually see

```text
none    counter =  2053689  expected 4000000  WRONG - updates were lost        4.5 ms
none    counter =  1069043  expected 4000000  WRONG - updates were lost        2.8 ms
mutex   counter =  4000000  expected 4000000  correct                        289.3 ms
atomic  counter =  4000000  expected 4000000  correct                         96.4 ms
local   counter =  4000000  expected 4000000  correct                          0.3 ms

thread 1: holds A, wants B
thread 2: holds B, wants A

DEADLOCK: no progress for 2s - each thread holds the lock the other needs (hold-and-wait + circular wait)

thread 2: holds A, wants B
thread 2: got both locks, doing work
thread 1: holds A, wants B
thread 1: got both locks, doing work
both threads finished - a single global lock order makes circular wait impossible
```

- **Half to three-quarters of the increments vanished.** Not occasionally - on every run.
- **mutex is ~65x slower than unsynchronized**, atomic ~20x - correctness has a price when
  every single operation is contended. **`local` is fastest *and* correct**: the best fix for
  contention is usually to stop sharing.

## Code walkthrough

`counter++` looks like one operation. It's three:

```text
thread A                    thread B
load  counter -> reg (=7)
                            load  counter -> reg (=7)
add   reg, 1     (=8)
                            add   reg, 1     (=8)
store reg -> counter (=8)
                            store reg -> counter (=8)    <- A's increment is lost
```

The fixes:

```c
pthread_mutex_lock(&lock);    // only one thread at a time inside the critical section
counter++;
pthread_mutex_unlock(&lock);

__atomic_fetch_add(&counter, 1, __ATOMIC_RELAXED);   // compiles to `lock xadd` on x86:
                                                     // the CPU does load+add+store as one
                                                     // indivisible step, locking the cache line
```

Note the comment on `volatile` in `race.c`: it stops the compiler from keeping `counter` in a
register, but it does **not** make `counter++` atomic. A very common misconception.

The deadlock needs all four **Coffman conditions** at once: mutual exclusion (a mutex),
hold-and-wait (holding A while waiting for B), no preemption (nobody can take A away), and
**circular wait** (1 waits for 2, 2 waits for 1). Making everyone take A before B breaks the
circle - that's the fix used in practice, e.g. the Linux kernel documents lock orderings and
checks them at runtime with **lockdep**.

## Try this yourself

- `make tsan` - ThreadSanitizer reports `data race` with both threads' stack traces.
- Change `THREADS` to 1 - `none` becomes correct. Races need concurrency.
- Pin the program to one CPU: `taskset -c 0 ./bin/race none`. It's still wrong (preemption
  can land between the load and the store), but much *less* wrong - why?
- In `deadlock.c`, replace `pthread_mutex_lock(second)` with `pthread_mutex_trylock` and back
  off (unlock `first`, sleep, retry) on failure. That breaks hold-and-wait instead.

## Real-world equivalent

- Lost updates are the same bug as two web requests doing `read balance → add → write
  balance` without a transaction or `SELECT ... FOR UPDATE`.
- Databases detect deadlocks between transactions and kill one of them; most programs don't, so
  they just hang - which is why `deadlock.c` needed its own watchdog.
- Per-thread/per-CPU counters combined on read (`local` mode) are how the Linux kernel keeps
  hot statistics (`percpu_counter`) and how Java's `LongAdder` beats `AtomicLong` under
  contention.
