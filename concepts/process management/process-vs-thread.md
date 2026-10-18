# Process vs Thread

A **process** is a container: an address space plus resources (open files, sockets, signal
handlers, an identity). A **thread** is a path of execution *inside* that container: a program
counter, a set of registers, and a stack. Every process starts with one thread; it can create
more, and they all share the process's memory.

## What's shared and what's private

| | Shared by all threads of a process | Private to each thread |
| --- | --- | --- |
| **Memory** | code (text), globals (data/bss), the **heap**, memory-mapped files | its own **stack** |
| **CPU state** | - | program counter, stack pointer, all registers |
| **Kernel resources** | file descriptors, sockets, current directory, signal handlers, PID | thread ID (TID), signal mask, scheduling priority, errno, thread-local storage |

Two processes share **nothing** by default (beyond read-only code pages the kernel quietly
deduplicates). Communicating between processes needs explicit IPC: pipes, sockets, shared
memory (`mmap(MAP_SHARED)`, `shm_open`), message queues, signals.

```text
 process A (pid 100)                         process B (pid 200)
 +--------------------------------+           +-------------------------+
 | code | globals | heap          |           | code | globals | heap   |
 |[stack T1] [stack T2] [stack T3]|           |  [stack T1]             |
 |   T1        T2        T3       |           |   T1                    |
 | fds: 0 1 2 3(socket) 4(file)   |           | fds: 0 1 2              |
 +--------------------------------+           +-------------------------+
   threads of A share everything                 A and B share nothing
   except their stacks and registers             (separate page tables)
```

🖼️ **threads inside a process:**
![process vs thread](<../../assets/img/process-vs-thread.png>)

## On Linux, both are "tasks"

The kernel doesn't really distinguish: both are `task_struct`s created with `clone()`. The flags
decide what's shared:

```c
fork()           ≈ clone(SIGCHLD)                               // share nothing, copy-on-write
pthread_create() ≈ clone(CLONE_VM | CLONE_FS | CLONE_FILES |
                         CLONE_SIGHAND | CLONE_THREAD | ...)    // share memory, fds, handlers
```

- `getpid()` returns the **thread group ID** - the same for every thread of a process.
- `gettid()` returns the task's own ID. The first thread's TID equals the PID.
- `ps -eLf` or `ls /proc/<pid>/task/` lists a process's threads.

Windows draws the line explicitly: a process is a container that's never scheduled itself;
threads are the schedulable units.

## The trade-offs

| | Processes | Threads |
| --- | --- | --- |
| **Isolation** | strong - a crash or memory corruption stays inside one process | none - one thread's bad pointer can corrupt or crash all of them |
| **Communication** | explicit IPC: copying, serialization, syscalls | just read/write shared memory - fast, but needs synchronization |
| **Creation cost** | higher: new address space, page-table copy, fd table copy | lower: a stack and a task struct |
| **Context-switch cost** | higher: page tables change (TLB flush unless tagged with PCID/ASID) | lower: same address space |
| **Memory overhead** | each has its own heap, page tables, libraries' data | one heap, one set of page tables |
| **Security boundary** | yes - different users, sandboxes, namespaces | no |
| **Bugs** | IPC protocol bugs | race conditions, deadlocks |

## Real systems, real choices

- **Chrome** - a process per site (renderer) for security and crash isolation.
- **PostgreSQL** - a process per connection (simplicity, isolation), hence connection poolers.
- **MySQL** - a thread per connection.
- **Nginx** - one process per core, each running an event loop.
- **Redis** - (mostly) one thread for commands, plus `fork()` for snapshots.
- **Go / Java virtual threads** - thousands of lightweight user-space "threads" multiplexed
  onto a few OS threads.

## Clarifications

- **A thread is not a "lightweight process" in every sense.** It's lightweight to *create* and
  *switch*, but it gives up isolation entirely.
- **Threads don't automatically mean parallelism.** On one core they're interleaved; in CPython
  the GIL prevents bytecode from running in parallel.
- **`fork()` in a multi-threaded program copies only the calling thread.** Locks held by other
  threads stay locked forever in the child - which is why the child should usually call `exec`
  right away.
- **User-level threads / green threads / coroutines** are scheduled by a runtime, not the kernel;
  the kernel only sees the OS threads underneath.

## Practice project

[`practice/process-management/process-vs-thread`](../../practice/process-management/process-vs-thread) -
a child process's write to a global is invisible to the parent while a thread's write isn't;
threads report the same PID and different TIDs; process creation measured at ~2.7x a thread; and
`fork()` slowing from 0.2ms to 6.9ms as the parent grows to 1GB of touched memory.

## Related

- [Context switching](context-switching.md) - why a thread switch is cheaper than a process switch.
- [Concurrency](concurrency.md) - the price of shared memory.
- [When do you use threads?](when-do-you-use-threads.md) - choosing between them.
- [Virtual memory](<../memory management/virtual-memory.md>) - the separate page tables that
  isolate processes.
