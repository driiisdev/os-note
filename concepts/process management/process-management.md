# Process Management - Section Intro

A typical machine has a handful of cores and hundreds of processes with thousands of threads.
**Process management** is how the OS creates them, decides which one runs on each core and for
how long, switches between them, and keeps them from corrupting each other when they share data.

## The pieces

```text
           fork()/clone()                  scheduler picks it
  [new] -----------------> [ready queue] ----------------------> [running on a core]
                               ^   ^                               |    |     |
                               |   +-- timer interrupt: preempt ---+    |     |
                               |                                        |     | exit()
                               +------ I/O done / lock free ---- [blocked] <--+     v
                                                                 (read, sleep,   [zombie]
                                                                  mutex wait)       |
                                                                         parent wait()
                                                                                   v
                                                                                 [gone]
```

- **Creation** - `fork()` + `execve()` on Unix, `CreateProcess()` on Windows; threads via
  `pthread_create()` (on Linux, both are `clone()` with different flags).
- **Scheduling** - choosing the next runnable thread for each core: Linux's CFS (and, since
  6.6, EEVDF) tries to give each thread a fair share of CPU time weighted by priority (`nice`).
- **Context switching** - saving one thread's CPU state and restoring another's.
- **Synchronization** - mutexes, semaphores, condition variables, atomics, so that threads
  sharing memory don't race.
- **Termination** - exit codes, zombies, `wait()`, signals like `SIGTERM`/`SIGKILL`.

🖼️ **Image to add - process lifecycle:** the state diagram above as a clean graphic - states as
rounded boxes (new, ready, running, blocked, zombie), transitions labeled with what causes them
(fork, scheduled, preempted, I/O wait, I/O done, exit, wait).
![process lifecycle](<../../assets/img/process-lifecycle.png>)

## What this section covers

| # | Topic | The question it answers |
| --- | --- | --- |
| 1 | [Process vs thread](process-vs-thread.md) | What's shared, what's private, what each costs |
| 2 | [Context switching](context-switching.md) | What happens when the OS swaps one thread for another, and what it costs |
| 3 | [Concurrency](concurrency.md) | Race conditions, locks, atomics, deadlocks |
| 4 | [Demo: process management](demo-process-management.md) | Watching processes, threads, states and switches on a real system |
| 5 | [When do you use threads?](when-do-you-use-threads.md) | How many threads, and for what kind of work |

## Scheduling in one paragraph

Every core runs a loop: pick a runnable thread, let it run until it **blocks** (I/O, a lock,
`sleep`), **yields**, or its **time slice** expires (a timer interrupt), then pick again. Linux
keeps a run queue per core, gives each thread a "virtual runtime" that grows as it runs (more
slowly for higher priority), and picks the thread that is most "owed" CPU time. Interactive
threads that mostly block get to run quickly when they wake. Real-time policies
(`SCHED_FIFO`, `SCHED_RR`) bypass fairness entirely for latency-critical work. Threads can be
pinned to specific cores with CPU affinity (`taskset`, `sched_setaffinity`).

## Practice projects

| Project | Concept |
| --- | --- |
| [process-vs-thread](../../practice/process-management/process-vs-thread) | memory sharing, PID vs TID, creation cost |
| [context-switch-cost](../../practice/process-management/context-switch-cost) | the price of a switch |
| [race-condition-lab](../../practice/process-management/race-condition-lab) | races, locks, atomics, deadlock |
| [when-threads-help](../../practice/process-management/when-threads-help) | thread scaling, CPU vs I/O-bound |
| [mini-shell](../../practice/process-management/mini-shell) | fork, exec, wait, pipes, signals - a working shell |

## Related

- [Anatomy of a process](<../anatomy of process/anatomy-of-a-process.md>) - what a single
  process is made of.
- [Inside the CPU](<../inside the cpu/inside-the-cpu.md>) - the cores being shared.
- [Socket programming patterns](<../socket management/socket-programming-patterns.md>) - process
  and thread models applied to servers.
