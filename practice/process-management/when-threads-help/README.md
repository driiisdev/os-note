# Practice: When Threads Help

Concept note: [`when-do-you-use-threads.md`](<../../../concepts/process management/when-do-you-use-threads.md>)

## What this demonstrates

The question "how many threads should I use?" answered with a measurement. 32 equal tasks run
on a pool of 1, 2, 4 ... 32 threads, for two kinds of work:

- **CPU-bound** - each task burns ~20ms of pure computation. More threads help only until
  every core is busy; after that, threads just take turns on the same cores.
- **I/O-bound** - each task sleeps ~20ms, standing in for waiting on a disk, a database or a
  network reply. A blocked thread uses no core at all, so more threads keep helping.

## Folder structure

```text
when-threads-help/
  threads.c    <- a tiny thread pool + work queue, calibrated busy-work, timed runs
  Makefile     <- `make`, `make demo`
```

## Build & run

```bash
make demo
./bin/threads cpu
./bin/threads io
```

## What you'll actually see

On 2 physical cores / 4 hardware threads:

```text
32 CPU-bound tasks of ~20ms each, 4 CPU cores online
  threads    total ms    speedup
  1               620       1.0x
  2               322       1.9x
  4               192       3.2x
  8               218       2.8x
  16              220       2.8x
  32              225       2.8x

32 I/O-bound (sleeping) tasks of ~20ms each, 4 CPU cores online
  threads    total ms    speedup
  1               648       1.0x
  2               323       2.0x
  4               162       4.0x
  8                81       8.0x
  16               41      15.7x
  32               23      28.5x
```

- **CPU-bound tops out at ~3.2x on "4 cores"** - because those are 2 physical cores with
  hyperthreading. Two hyperthreads share one core's execution units, so they give much less
  than 2x. Past 4 threads it gets slightly *worse*: more context switches, same compute.
- **I/O-bound scales almost perfectly to 32x** - the waiting overlaps, and the CPU was never
  the bottleneck.

## Code walkthrough

The pool is just N threads pulling task numbers from a shared atomic counter - the simplest
possible work queue:

```c
static void *worker(void *arg) {
    while (__atomic_fetch_add(&next_task, 1, __ATOMIC_RELAXED) < TASKS) {
        if (io_mode) nanosleep(&d, NULL);   // blocked: the scheduler gives the core away
        else burn(spin_per_ms * TASK_MS);   // running: this thread needs a core the whole time
    }
    return NULL;
}
```

Busy-work is calibrated at startup ("how many loop iterations make one millisecond on this
machine?"), so a task is ~20ms on any CPU.

## Try this yourself

- `nproc` vs `lscpu | grep -E 'Core|Thread'` - how many of your "cores" are real?
- Make a **mixed** workload: 10ms compute + 10ms sleep per task. Where does it stop scaling?
  (Rule of thumb: optimal threads ≈ cores × (1 + wait time / compute time).)
- Increase `TASKS` to 1024 and run 1024 threads in I/O mode. It still works - but run
  `cat /proc/<pid>/status | grep -E 'Threads|VmRSS'` while it runs to see what each thread's
  stack costs.

## Real-world equivalent

- **Thread pool sizing**: Java's `ForkJoinPool` defaults to the number of cores (CPU-bound
  work), while database connection pools and HTTP client pools are sized by how much waiting
  is involved.
- **Node.js** runs JavaScript on one thread but uses a 4-thread pool (libuv) for blocking file
  and DNS operations - I/O-bound work that would otherwise block the event loop.
- **Python's GIL** makes CPU-bound threads useless for speedup (only one runs bytecode at a
  time) while I/O-bound threads still help - the two halves of this demo, separated by policy.
