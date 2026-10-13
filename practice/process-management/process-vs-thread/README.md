# Practice: Process vs Thread

Concept note: [`process-vs-thread.md`](<../../../concepts/process management/process-vs-thread.md>)

## What this demonstrates

Three claims from the concept note, measured instead of asserted:

1. **Memory** - a child *process* gets its own copy of the address space, so its writes are
   invisible to the parent. A *thread* shares the address space, so its writes are visible
   immediately.
2. **Identity** - threads share one process ID (PID) but each gets its own thread ID (TID).
   To the Linux kernel, both are "tasks"; a thread is just a task that shares more.
3. **Cost** - creating a process costs more than creating a thread, and `fork()` gets *slower
   as the parent gets bigger*, because the kernel has to copy the parent's page tables.

## Folder structure

```text
process-vs-thread/
  pvt.c       <- all three experiments
  Makefile    <- `make`
```

## Build & run

```bash
make
./bin/pvt
```

## What you'll actually see

```text
1) who sees the write?
   main   : pid 627, tid 627 - counter = 0
   child  : pid 628, tid 628 - incremented counter to 1
   main   : after the child process  -> counter = 0 (child changed its own copy)
   thread : pid 627, tid 629 - incremented counter to 1
   main   : after the thread         -> counter = 1 (thread changed OUR memory)

2) creation cost, averaged over 2000 create+exit+wait cycles
   thread :   91.9 us
   process:  244.1 us   (2.7x a thread)

3) fork() copies the parent's page tables - so it costs more as the parent grows
   parent with    0 MB touched: fork ~  207.8 us
   parent with  256 MB touched: fork ~  649.6 us
   parent with 1024 MB touched: fork ~ 6880.4 us
```

- The main thread's **TID equals the PID** (627) - the first task in a process defines its ID.
  The new thread is TID 629 but still PID 627. The child process got a new PID (628).
- **2.7x**: a thread needs a new stack and a kernel task structure. A process additionally
  needs a new address space (`mm_struct`), copied page tables, a copied file-descriptor table,
  and copy-on-write setup for every page.
- **0.2ms → 6.9ms**: with 1GB touched, the parent has ~262,000 page-table entries for `fork()`
  to copy and mark read-only. This is why `fork()` from a huge process is slow, and why
  `posix_spawn()`/`vfork()` exist.

(Your numbers will differ - WSL2 runs under Hyper-V, which makes process creation a bit more
expensive than on bare metal. The *ratios* are what matter.)

## Code walkthrough

```c
int counter = 0;   // one global, in .data

pid_t pid = fork();
if (pid == 0) {           // child: its OWN copy of .data (copy-on-write)
    counter += 1;
    _exit(0);
}
waitpid(pid, NULL, 0);    // parent: still 0

pthread_create(&th, NULL, increment, NULL);  // thread: SAME .data
pthread_join(th, NULL);                      // main now sees 1
```

`gettid()` returns the kernel's ID for the calling task; `getpid()` returns the thread
*group* ID - that's the "process". On Linux, `pthread_create` is implemented with `clone()`
using flags like `CLONE_VM | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD` ("share memory, share
fds, share signal handlers, same thread group"). `fork()` is `clone()` with none of them.

## Try this yourself

- `ps -eLf | head` lists every *thread* on the system (`LWP` column = TID). Run a program with
  threads and find its PID repeated on several lines.
- `ls /proc/<pid>/task/` - each thread is a subdirectory.
- Change the `fork()` cost test to use `vfork()` (child must call `_exit` immediately). The
  page-table copy disappears - the cost stays flat no matter how big the parent is.
- Replace the thread's `counter += 1` with a loop of 1,000,000 increments in two threads -
  then head to [race-condition-lab](../race-condition-lab) to see why the answer is wrong.

## Real-world equivalent

- **Postgres** uses a process per connection (crash isolation, simpler memory model), which is
  why connection poolers like PgBouncer exist - creating a process per connection is expensive.
  **MySQL** uses a thread per connection.
- **Chrome** runs each site in its own process for security isolation, accepting the memory cost.
- **Nginx** runs a few worker *processes* (one per core), each with an event loop.
- **Redis** `BGSAVE` deliberately uses `fork()` to get a copy-on-write snapshot of its memory -
  and its docs warn about fork latency on large datasets, which is experiment 3.
