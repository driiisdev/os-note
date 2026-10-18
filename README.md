# OS - Notes

Personal study notes for the **Fundamentals of Operating Systems** course (Udemy) - raw lecture
notes turned into proper write-ups, plus small hands-on projects for each concept, so the ideas
stick beyond just reading about them. The goal is a working mental model of what the OS actually
does underneath every program: how a process is laid out in memory, how the CPU runs it, how
memory, storage and the network are shared and protected - and what each of those costs.

## How notes are organized

```text
concepts/
  <section name>/             <- matches a course section (e.g. "memory management")
    topic.txt                  <- raw notes taken live, during the lecture
    topic.md                   <- write-up: explanation, worked examples, text diagrams,
                                  🖼️ image callouts, clarifications, links to practice + related notes
practice/
  <section-name>/              <- kebab-case, these are project folders
    <project>/
      README.md                <- what it demonstrates, folder structure, how to run,
                                  real captured output, code walkthrough, things to try,
                                  and the real-world system that does the same thing
      *.c / *.py, Makefile
assets/
  img/                         <- diagrams and screenshots referenced by the notes
```

**Why a `.txt` *and* a `.md` for the same topic?** The `.txt` stays as the honest, messy,
in-the-moment note - good for review. The `.md` is the "I actually understand this now" version,
written after connecting it to how a real kernel behaves and to code that runs.

**Why do some `practice/` projects cover several topics?** Some concepts only make sense side by
side (the stack, the heap and the data section are all answers to "where does this variable
live?"), so one project exercises them together. The `practice/<section-name>` folder always
matches its `concepts/<section name>` folder (hyphenated instead of spaced).

**About the folder names**: folder and file names keep the original spelling from when the notes
were created (e.g. `os into`, `anatomy of process`) - the links in this repo depend on them.

**Where the content came from**: `os into/os-intro.txt` had real notes; every other `.txt` is
still empty, so those `.md` write-ups were written from the lecture titles and the course's
subject matter. As each `.txt` gets filled in, reconcile the `.md` with it - the demo notes in
particular each say where the lecture's exact steps should be added.

## Sections

| Section | Topics | Practice projects |
| --- | --- | --- |
| [OS intro](<concepts/os into>) | 1 | 1 |
| [Anatomy of a process](<concepts/anatomy of process>) | 8 | 2 |
| [Memory management](<concepts/memory management>) | 6 | 2 |
| [Inside the CPU](<concepts/inside the cpu>) | 5 | 2 |
| [Process management](<concepts/process management>) | 6 | 5 |
| [Storage management](<concepts/storage management>) | 5 | 2 |
| [Socket management](<concepts/socket management>) | 6 | 2 |
| [OS concepts](<concepts/os concepts>) | 9 | 7 |

As each course section gets covered, follow the same loop: write the raw `.txt` during the
lecture, turn it into the `.md`, then build (or extend) the matching project under `practice/`.

## OS intro - topics

Why an OS exists at all (abstraction, resource management, protection) and the hardware it's
managing - CPU, memory, storage, network - with the latency numbers that drive every design
decision later in the course.

| # | Topic | Note | Practice project |
| --- | --- | --- | --- |
| 1 | Section intro, why do we need an OS, system architecture overview | [os-intro.md](<concepts/os into/os-intro.md>) | [hello-three-ways](practice/os-into/hello-three-ways) |

## Anatomy of a process - topics

What the OS builds when you run a program: the program file vs the running process, how the CPU
steps through instructions, and the four memory regions every process is made of.

| # | Topic | Note | Practice project |
| --- | --- | --- | --- |
| 1 | Section intro | [anatomy-of-a-process.md](<concepts/anatomy of process/anatomy-of-a-process.md>) | [process-memory-map](practice/anatomy-of-process/process-memory-map), [stack-frames](practice/anatomy-of-process/stack-frames) |
| 2 | Program vs process | [program-vs-process.md](<concepts/anatomy of process/program-vs-process.md>) | [process-memory-map](practice/anatomy-of-process/process-memory-map) |
| 3 | Simple process execution | [simple-process-execution.md](<concepts/anatomy of process/simple-process-execution.md>) | [stack-frames](practice/anatomy-of-process/stack-frames) (`make asm`) |
| 4 | The stack | [the-stack.md](<concepts/anatomy of process/the-stack.md>) | [stack-frames](practice/anatomy-of-process/stack-frames) |
| 5 | Process execution with stack | [process-execution-with-stack.md](<concepts/anatomy of process/process-execution-with-stack.md>) | [stack-frames](practice/anatomy-of-process/stack-frames) (`make step`) |
| 6 | Data section | [data-section.md](<concepts/anatomy of process/data-section.md>) | [process-memory-map](practice/anatomy-of-process/process-memory-map) |
| 7 | The heap | [the-heap.md](<concepts/anatomy of process/the-heap.md>) | [process-memory-map](practice/anatomy-of-process/process-memory-map) (`heap`) |
| 8 | Process section demo | [process-section-demo.md](<concepts/anatomy of process/process-section-demo.md>) | both of the above |

## Memory management - topics

What's underneath the virtual addresses from the previous section: how RAM is built, the path a
load or store takes through the caches, how every process gets a private address space, and how
devices write into memory without the CPU.

| # | Topic | Note | Practice project |
| --- | --- | --- | --- |
| 1 | Section intro | [memory-management.md](<concepts/memory management/memory-management.md>) | - |
| 2 | The anatomy of memory | [the-anatomy-of-memory.md](<concepts/memory management/the-anatomy-of-memory.md>) | [cache-locality](practice/memory-management/cache-locality) |
| 3 | Reading and writing from and to memory | [reading-n-writing-from-n-to-memory.md](<concepts/memory management/reading-n-writing-from-n-to-memory.md>) | [cache-locality](practice/memory-management/cache-locality) |
| 4 | Virtual memory | [virtual-memory.md](<concepts/memory management/virtual-memory.md>) | [virtual-memory-lab](practice/memory-management/virtual-memory-lab) |
| 5 | DMA | [dma.md](<concepts/memory management/dma.md>) | [file-io-journey](practice/storage-management/file-io-journey) (`O_DIRECT`) - DMA itself happens inside drivers |
| 6 | Memory section demo | [memory-section-demo.md](<concepts/memory management/memory-section-demo.md>) | [virtual-memory-lab](practice/memory-management/virtual-memory-lab), [cache-locality](practice/memory-management/cache-locality) |

## Inside the CPU - topics

The hardware the scheduler is sharing out: what's in a core, the life of one instruction, how
pipelines and out-of-order execution run many at once, and how much time a CPU spends waiting.

| # | Topic | Note | Practice project |
| --- | --- | --- | --- |
| 1 | Section intro | [inside-the-cpu.md](<concepts/inside the cpu/inside-the-cpu.md>) | - |
| 2 | CPU components and architecture | [cpu-components-n-architecture.md](<concepts/inside the cpu/cpu-components-n-architecture.md>) | [toy-cpu](practice/inside-the-cpu/toy-cpu) |
| 3 | Instruction life cycle | [instruction-life-cycle.md](<concepts/inside the cpu/instruction-life-cycle.md>) | [toy-cpu](practice/inside-the-cpu/toy-cpu) |
| 4 | Pipelining and parallelism | [pipelining-n-parallelism.md](<concepts/inside the cpu/pipelining-n-parallelism.md>) | [cpu-wait-times](practice/inside-the-cpu/cpu-wait-times) (`branch`, `ilp`) |
| 5 | Inside the CPU demo (CPU wait times) | [inside-the-cpu-demo-cpu-wait-times.md](<concepts/inside the cpu/inside-the-cpu-demo-cpu-wait-times.md>) | [cpu-wait-times](practice/inside-the-cpu/cpu-wait-times) (`latency`) |

## Process management - topics

How the OS runs many processes and threads on a few cores: what's shared and what isn't, what a
context switch costs, why shared memory needs locks, and how many threads are worth having.

| # | Topic | Note | Practice project |
| --- | --- | --- | --- |
| 1 | Section intro | [process-management.md](<concepts/process management/process-management.md>) | - |
| 2 | Process vs thread | [process-vs-thread.md](<concepts/process management/process-vs-thread.md>) | [process-vs-thread](practice/process-management/process-vs-thread) |
| 3 | Context switching | [context-switching.md](<concepts/process management/context-switching.md>) | [context-switch-cost](practice/process-management/context-switch-cost) |
| 4 | Concurrency | [concurrency.md](<concepts/process management/concurrency.md>) | [race-condition-lab](practice/process-management/race-condition-lab) |
| 5 | Demo: process management | [demo-process-management.md](<concepts/process management/demo-process-management.md>) | [mini-shell](practice/process-management/mini-shell) |
| 6 | When do you use threads? | [when-do-you-use-threads.md](<concepts/process management/when-do-you-use-threads.md>) | [when-threads-help](practice/process-management/when-threads-help) |

## Storage management - topics

How a slow, block-addressed device becomes files and directories, why the page cache makes it
feel fast, and what it really takes for data to be safe on disk.

| # | Topic | Note | Practice project |
| --- | --- | --- | --- |
| 1 | Section intro | [storage-management.md](<concepts/storage management/storage-management.md>) | - |
| 2 | Persistent storage | [persistent-storage.md](<concepts/storage management/persistent-storage.md>) | [file-io-journey](practice/storage-management/file-io-journey) |
| 3 | File systems | [file-systems.md](<concepts/storage management/file-systems.md>) | [toy-filesystem](practice/storage-management/toy-filesystem) |
| 4 | What really happens in a file I/O? | [what-really-happens-in-a-file-io.md](<concepts/storage management/what-really-happens-in-a-file-io.md>) | [file-io-journey](practice/storage-management/file-io-journey) |
| 5 | Storage management demo | [storage-management-demo.md](<concepts/storage management/storage-management-demo.md>) | [file-io-journey](practice/storage-management/file-io-journey) |

## Socket management - topics

What the kernel does behind a socket - handshakes, queues, buffers - and how servers are
structured to handle many connections: processes, threads, event loops, async I/O.

| # | Topic | Note | Practice project |
| --- | --- | --- | --- |
| 1 | Section intro | [socket-management.md](<concepts/socket management/socket-management.md>) | - |
| 2 | Sockets, connections and kernel queues | [sockets-connections-n-kernel-queues.md](<concepts/socket management/sockets-connections-n-kernel-queues.md>) | [socket-buffers](practice/socket-management/socket-buffers) (`backlog`) |
| 3 | Sending and receiving data | [sending-n-receiving-data.md](<concepts/socket management/sending-n-receiving-data.md>) | [socket-buffers](practice/socket-management/socket-buffers) (`buffers`) |
| 4 | Socket programming patterns | [socket-programming-patterns.md](<concepts/socket management/socket-programming-patterns.md>) | [tcp-server-patterns](practice/socket-management/tcp-server-patterns) |
| 5 | Asynchronous IO | [asynchronous-io.md](<concepts/socket management/asynchronous-io.md>) | [tcp-server-patterns](practice/socket-management/tcp-server-patterns) (`epoll`) |
| 6 | Socket management demo | [socket-management-demo.md](<concepts/socket management/socket-management-demo.md>) | both of the above |

## OS concepts - topics

Cross-cutting foundations (building and loading programs, privilege modes, VMs and containers,
page tables) plus case studies where those internals decided a real outcome.

| # | Topic | Note | Practice project |
| --- | --- | --- | --- |
| 1 | Section intro | [os-concepts.md](<concepts/os concepts/os-concepts.md>) | - |
| 2 | Compilers and linkers | [compilers-n-linkers.md](<concepts/os concepts/compilers-n-linkers.md>) | [build-pipeline](practice/os-concepts/build-pipeline) |
| 3 | Kernel vs user mode switching | [kernel-vs-user-mode-switching.md](<concepts/os concepts/kernel-vs-user-mode-switching.md>) | [syscall-cost](practice/os-concepts/syscall-cost) |
| 4 | Virtualization and containerization | [virtualization-n-containerization.md](<concepts/os concepts/virtualization-n-containerization.md>) | [mini-container](practice/os-concepts/mini-container) (needs sudo) |
| 5 | How Google improved Linux TCP/IP stack by 40% | [how-google-improved-linux-tcp-ip-stack-by-40-percent.md](<concepts/os concepts/how-google-improved-linux-tcp-ip-stack-by-40-percent.md>) | [struct-cacheline-layout](practice/os-concepts/struct-cacheline-layout) |
| 6 | How TikTok's ByteDance improved Linux reboot | [how-tiktok-bytedance-improved-linux-reboot.md](<concepts/os concepts/how-tiktok-bytedance-improved-linux-reboot.md>) | [kexec-reboot-sim](practice/os-concepts/kexec-reboot-sim) |
| 7 | On timeouts | [on-timeouts.md](<concepts/os concepts/on-timeouts.md>) | [timeouts-lab](practice/os-concepts/timeouts-lab) |
| 8 | Postgres failure caused by a Cisco router (TCP issue) | [postgres-failure-caused-by-a-cisco-router-tcp-issue.md](<concepts/os concepts/postgres-failure-caused-by-a-cisco-router-tcp-issue.md>) | [timeouts-lab](practice/os-concepts/timeouts-lab) (scenario 3) |
| 9 | Page tables | [page-tables.md](<concepts/os concepts/page-tables.md>) | [page-table-walker](practice/os-concepts/page-table-walker) |

## Learning roadmap

A suggested order, based on what depends on what (close to, but not exactly, the course order):

1. **Why the OS exists** - [OS intro](<concepts/os into/os-intro.md>). The three jobs
   (abstraction, resource management, protection) and the latency table are the lens for
   everything after.
2. **What a running program is** - [program vs process](<concepts/anatomy of process/program-vs-process.md>) →
   [simple process execution](<concepts/anatomy of process/simple-process-execution.md>) →
   [the stack](<concepts/anatomy of process/the-stack.md>) →
   [process execution with stack](<concepts/anatomy of process/process-execution-with-stack.md>) →
   [data section](<concepts/anatomy of process/data-section.md>) →
   [the heap](<concepts/anatomy of process/the-heap.md>). Build `stack-frames` and
   `process-memory-map` here - seeing real addresses makes the rest concrete.
3. **The hardware running it** - [CPU components](<concepts/inside the cpu/cpu-components-n-architecture.md>) →
   [instruction life cycle](<concepts/inside the cpu/instruction-life-cycle.md>) →
   [pipelining and parallelism](<concepts/inside the cpu/pipelining-n-parallelism.md>). Reading
   this right after step 2 connects the assembly you just saw to what the core does with it.
4. **Memory, physically and virtually** - [anatomy of memory](<concepts/memory management/the-anatomy-of-memory.md>) →
   [reading and writing memory](<concepts/memory management/reading-n-writing-from-n-to-memory.md>) →
   [CPU wait times](<concepts/inside the cpu/inside-the-cpu-demo-cpu-wait-times.md>) →
   [virtual memory](<concepts/memory management/virtual-memory.md>) →
   [page tables](<concepts/os concepts/page-tables.md>). The cache and TLB ideas here explain
   most performance surprises later.
5. **The boundary between programs and the kernel** - [kernel vs user mode](<concepts/os concepts/kernel-vs-user-mode-switching.md>).
   Short, but every later section involves crossing it.
6. **Running many things at once** - [process vs thread](<concepts/process management/process-vs-thread.md>) →
   [context switching](<concepts/process management/context-switching.md>) →
   [concurrency](<concepts/process management/concurrency.md>) →
   [when do you use threads?](<concepts/process management/when-do-you-use-threads.md>). Then
   build `mini-shell` - it uses almost everything from steps 2-6.
7. **Devices: storage** - [DMA](<concepts/memory management/dma.md>) →
   [persistent storage](<concepts/storage management/persistent-storage.md>) →
   [file systems](<concepts/storage management/file-systems.md>) →
   [what really happens in a file I/O](<concepts/storage management/what-really-happens-in-a-file-io.md>).
8. **Devices: the network** - [sockets and kernel queues](<concepts/socket management/sockets-connections-n-kernel-queues.md>) →
   [sending and receiving data](<concepts/socket management/sending-n-receiving-data.md>) →
   [asynchronous I/O](<concepts/socket management/asynchronous-io.md>) →
   [socket programming patterns](<concepts/socket management/socket-programming-patterns.md>).
   Step 6's thread costs are exactly why event loops exist.
9. **How programs are built and packaged** - [compilers and linkers](<concepts/os concepts/compilers-n-linkers.md>) →
   [virtualization and containerization](<concepts/os concepts/virtualization-n-containerization.md>).
10. **Case studies, once the foundations click** -
    [Google's TCP stack](<concepts/os concepts/how-google-improved-linux-tcp-ip-stack-by-40-percent.md>) (needs step 4),
    [ByteDance's kexec reboot](<concepts/os concepts/how-tiktok-bytedance-improved-linux-reboot.md>) (needs steps 4 and 9),
    [on timeouts](<concepts/os concepts/on-timeouts.md>) and
    [the Postgres/Cisco incident](<concepts/os concepts/postgres-failure-caused-by-a-cisco-router-tcp-issue.md>) (need step 8).

## Best practices for learning OS fundamentals

- **Always ask "what does it cost?"** A syscall (~60ns), a context switch (~1-3µs), a cache miss
  (~100ns), a page fault (~1-3µs), an `fsync` (milliseconds). Knowing the order of magnitude
  predicts most design decisions - why buffers, caches, thread pools and event loops exist.
- **Look at the real thing.** `/proc/<pid>/maps`, `/proc/<pid>/status`, `ss`, `vmstat`, `free`,
  `strace`, `gdb` show what the kernel is actually doing. Every note links to at least one tool or
  project that does this.
- **Read the assembly, at least once.** `gcc -O0 -S` for a five-line function demystifies the
  stack, calling conventions and "what the CPU runs" in a way no diagram does.
- **Measure, then explain.** Every practice project prints numbers. When a number surprises you
  (cross-core context switches being *slower*; three servers equally fast until the protocol
  changed), that's where the real understanding is.
- **Distrust benchmarks on one machine, trust ratios more than absolutes.** Everything here ran
  under WSL2 on a laptop; your numbers will differ, but which option is faster - and roughly by
  how much - should hold.
- **Learn the defaults and where they come from.** 4KB pages, 8MB stacks, a 2-hour TCP keepalive,
  `somaxconn`, `ulimit -n`. Many production incidents are a default nobody knew about.
- **Connect every topic back to a real system.** Postgres (process per connection, page tables,
  keepalives), Redis (fork + copy-on-write), nginx (event loops), Docker (namespaces + cgroups) -
  each project README ends with one.
- **Write the `.md` after building the project, not before.** The edge cases you hit in code (the
  unflushed `printf` before `_exit`, the pipe end left open, the kernel buffering every client's
  data) are the parts worth writing down.

## Practice projects

All projects are under [`practice/`](practice) - see [practice/README.md](practice/README.md) for
requirements and the full list. In short:

- **Linux required** - on Windows, use **WSL2 Ubuntu** and work from your Linux home directory
  (`cp -r` the repo there), not `/mnt/c`.
- `sudo apt install build-essential` (gcc + make); `python3` for the two Python projects; optional
  `gdb strace valgrind iptables dwarves` for the deeper "try this" steps.
- Each C project: `cd practice/<section>/<project> && make && make demo`.
- Every README includes **output captured from a real run** (WSL2 Ubuntu 24.04, gcc 13, Intel
  i5-8365U). Two scenarios that need `sudo` (`mini-container`, `timeouts-lab` scenario 3) were
  built but not run end-to-end; their READMEs say which output is expected rather than captured.
