# When Do You Use Threads?

Threads are a tool with real costs - memory for stacks, context switches, and the whole category
of [concurrency bugs](concurrency.md). They pay off in a few specific situations, and the right
number of them depends on **what the threads spend their time doing**.

## Good reasons to use threads

### 1. Parallelism for CPU-bound work

Work that can be split into independent chunks - image processing, compression, compiling,
number crunching - runs faster on more cores, **up to the number of cores**.

- Rule of thumb: **threads ≈ number of physical cores** (hyperthreads add ~10-30%, not 2x).
- More threads than cores gains nothing; they just take turns and add context switches.

### 2. Overlapping blocking I/O

A thread waiting on a disk read, a database query or an HTTP response uses no CPU. With more
threads, more waits overlap.

- Rule of thumb: **threads ≈ cores × (1 + wait time / compute time)**. A task that waits 90ms
  for every 10ms of CPU can usefully run ~10 threads per core.
- Beyond some point, memory (a stack per thread, 8MB virtual / tens of KB resident) and
  scheduling overhead dominate - which is why very high concurrency moves to
  [async I/O](<../socket management/asynchronous-io.md>) instead.

### 3. Keeping something responsive

Move slow work off a thread that must react quickly: the **UI thread** in a desktop/mobile app,
the **event loop** in Node.js, the **accept loop** in a server, an audio callback.

### 4. Isolating different kinds of work

Dedicated threads for background jobs: a logger, a flusher, a garbage collector, a metrics
reporter, a connection-pool health checker.

## When threads are the wrong tool

| Situation | Better alternative |
| --- | --- |
| thousands of mostly-idle connections | an **event loop** with epoll/kqueue/io_uring, or green threads (Go goroutines, Java virtual threads) |
| CPU-bound work in CPython | multiple **processes** (`multiprocessing`) - the GIL serializes Python bytecode |
| tasks that need fault or security isolation | **processes** (a crash or memory corruption stays contained) |
| a tiny amount of work per item | batching on one thread - creating/handing off costs more than the work |
| everything shares one hot lock | redesign to avoid sharing first; threads will just queue on the lock |

## Thread pools, not thread-per-task

Creating a thread costs tens of microseconds; reusing one costs nothing. Real systems create a
**pool** once and feed it a queue of tasks:

```text
 producers --> [ task queue ] --> worker 1
                              --> worker 2      (N fixed workers, sized for the workload)
                              --> worker N
```

Java's `ExecutorService`, .NET's thread pool, libuv's worker pool (4 threads by default, for file
I/O and DNS in Node.js), Rust's `rayon`, Go's runtime scheduler - all variations on this.

🖼️ **Image to add - thread scaling curves:** a chart of speedup vs number of threads (1-32):
the CPU-bound line rising until the core count and then flattening, the I/O-bound line rising
almost linearly - using the numbers from the practice project.
![thread scaling](<../../assets/img/thread-scaling.png>)

## Measured

From the practice project, on 2 cores / 4 hardware threads, 32 tasks of ~20ms each:

| Threads | CPU-bound speedup | I/O-bound speedup |
| --- | --- | --- |
| 1 | 1.0x | 1.0x |
| 2 | 1.9x | 2.0x |
| 4 | 3.2x | 4.0x |
| 8 | 2.8x | 8.0x |
| 32 | 2.8x | 28.5x |

CPU-bound work stops improving at the core count (and only reaches ~3.2x on "4 CPUs" because two
of them are hyperthreads); I/O-bound work keeps scaling.

## Clarifications

- **"More threads = faster" is only true for waiting.** For computing, the core count is a hard
  ceiling.
- **Threads don't fix a blocked event loop if the work stays on the loop.** The work has to
  actually move to the other thread.
- **Every extra thread is another interleaving to reason about.** Prefer the smallest number of
  threads that achieves the goal, and keep shared state to a minimum.

## Practice project

[`practice/process-management/when-threads-help`](../../practice/process-management/when-threads-help) -
the table above, generated on your machine: a work queue run with 1-32 threads for CPU-bound and
I/O-bound (sleeping) tasks.

## Related

- [Process vs thread](process-vs-thread.md) - what you get and give up with a thread.
- [Context switching](context-switching.md) - the cost that grows with too many threads.
- [Socket programming patterns](<../socket management/socket-programming-patterns.md>) - thread
  per connection vs event loops for servers.
