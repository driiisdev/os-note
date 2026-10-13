# OS Intro: Why an OS, and the System Architecture Underneath

The raw notes for this section are three lines - *section intro, why do we need an operating
system, system architecture overview* - so this write-up fills in both questions: **what job
the OS actually does**, and **what hardware it's doing that job on**. Everything else in the
course is a zoom-in on one piece of the picture below.

## Why do we need an operating system?

A computer without an OS is just hardware: a CPU that runs whatever instructions it's pointed
at, memory with no idea *whose* bytes it holds, and devices that each speak their own
register-level protocol. The OS makes that hardware **safe to share**, **convenient to
program**, and **fair to use** when many programs want it at once.

### Job 1: abstraction - hide the hardware

Programs don't talk to an NVMe controller; they call `read()` on a file. They don't program a
NIC's DMA ring; they `send()` on a socket. The OS turns messy, device-specific hardware into a
handful of uniform abstractions:

| Hardware reality | OS abstraction | Course section |
| --- | --- | --- |
| CPU cores, timer interrupts | **processes and threads** | anatomy of a process, process management |
| physical RAM chips | **virtual address spaces** | memory management |
| disk sectors / flash pages | **files and directories** | storage management |
| network interface card | **sockets** | socket management |
| keyboard, screen, serial line | **file descriptors / terminals** | - |

### Job 2: resource management - share finite hardware

There are almost always more runnable programs than cores, more requested memory than RAM, and
many programs hitting the same disk. The OS decides:

- **who runs on which CPU, and for how long** - the scheduler;
- **which physical pages back which process's memory** - the memory manager;
- **what order disk and network I/O happen in** - I/O schedulers, network queues.

### Job 3: protection and isolation - stop programs hurting each other

A bug in your editor must not corrupt your browser's memory, and an ordinary user's program
must not reprogram the disk controller. The OS enforces this *with hardware help*:

- **CPU privilege modes** - only kernel code can run privileged instructions
  ([kernel vs user mode](<../os concepts/kernel-vs-user-mode-switching.md>)).
- **Virtual memory** - each process sees only its own address space
  ([virtual memory](<../memory management/virtual-memory.md>)).
- **Permissions** - users, groups, file modes, capabilities.

### Kernel vs "the OS"

- **Kernel** - the privileged core, running in kernel mode: scheduler, memory manager, file
  systems, network stack, device drivers.
- **User space** - everything else that ships with the system: the C library (`glibc`, `musl`,
  Windows' `ntdll`/`kernel32`), shells, the init system, utilities.

Applications reach the kernel only through **system calls** - a deliberately narrow doorway.

```text
 +--------------------------------------------------+
 |  applications: bash, nginx, chrome, your code    |   user mode
 +--------------------------------------------------+
 |  libraries: libc (printf, malloc, fopen ...)     |   user mode
 +==================================================+  <- system call boundary
 | kernel: scheduler | memory mgr | VFS | net stack |   kernel mode
 |          device drivers                          |
 +--------------------------------------------------+
 |  hardware: CPU, RAM, disk, NIC, timers           |
 +--------------------------------------------------+
```

🖼️ **OS layers:**
![OS layers](<../../assets/img/os-layers.png>)

### Kernel designs

| Design | Idea | Examples |
| --- | --- | --- |
| **Monolithic** | all core services (FS, drivers, networking) in kernel space, in one binary (loadable modules allowed) | Linux, FreeBSD |
| **Microkernel** | kernel does only IPC, scheduling and basic memory management; drivers and file systems are user-space servers | seL4, QNX, MINIX 3 |
| **Hybrid** | microkernel-shaped, but many services still run in kernel space for speed | Windows NT, macOS (XNU) |

The trade-off: in a monolithic kernel, subsystems talk via plain function calls (fast), but a
buggy driver can crash everything. A microkernel isolates failures but pays for extra
IPC and context switches.

## System architecture overview

The OS manages four kinds of resources. It's worth having a rough mental picture of each, and
of **how far apart they are in time**, because almost every design decision in an OS is about
hiding the slow ones behind the fast ones.

```text
            +------------------------- CPU package --------------------------+
            |  core 0                 core 1                                 |
            |  [registers]            [registers]                            |
            |  [L1i][L1d]             [L1i][L1d]     ~1 ns                   |
            |  [  L2   ]              [  L2   ]      ~4 ns                   |
            |  [============= shared L3 cache ==============]  ~10-40 ns     |
            |  [ memory controller ]  [ PCIe root complex ]                  |
            +--------|------------------------|------------------------------+
                     |                        |
               +-----------+       +----------+----------+
               | DRAM (RAM)|       |  NVMe SSD |   NIC   |    devices DMA into RAM,
               |  ~100 ns  |       |  ~10-100us|  ~us-ms |    then interrupt the CPU
               +-----------+       +-----------+---------+
```

| Component | What it is | What the OS does with it |
| --- | --- | --- |
| **CPU** | cores (often 2 hardware threads each), registers, caches, MMU | schedules threads onto cores; switches between them; handles interrupts |
| **Memory (RAM)** | DRAM, volatile, ~100ns away | gives every process a virtual address space mapped onto physical pages |
| **Storage** | SSD/HDD, persistent, microseconds to milliseconds away | file systems, the page cache, I/O scheduling |
| **Network** | NIC, connected to other machines | the TCP/IP stack, sockets, kernel buffers and queues |

Approximate latencies (order-of-magnitude, they vary by hardware):

| Operation | Time | If 1 ns were 1 second |
| --- | --- | --- |
| register access / simple instruction | ~0.3 ns | 0.3 s |
| L1 cache hit | ~1 ns | 1 s |
| L2 cache hit | ~4 ns | 4 s |
| L3 cache hit | ~10-40 ns | ~20 s |
| RAM access | ~80-150 ns | ~2 min |
| syscall round trip | ~50-100 ns | ~1.5 min |
| context switch | ~1-5 µs | ~1 hour |
| NVMe SSD read | ~10-100 µs | ~1 day |
| network round trip, same datacenter | ~500 µs | ~6 days |
| HDD seek | ~5-10 ms | ~3 months |
| network round trip, across continents | ~100-150 ms | ~4 years |

You'll measure several of these rows yourself in the practice projects (cache latencies in
`cpu-wait-times`, syscalls in `syscall-cost`, context switches in `context-switch-cost`,
storage in `file-io-journey`).

🖼️ **system architecture:**
![system architecture](<../../assets/img/system-architecture.png>)

**How the pieces talk:**

- The CPU reads and writes **RAM** through its caches and memory controller.
- **Devices** are controlled by reading/writing their registers (memory-mapped I/O), move data
  in and out of RAM themselves via **DMA**, and announce completion with an **interrupt**
  ([DMA](<../memory management/dma.md>)).
- The **kernel** sits in the middle of all of it: it programs the devices, owns the page
  tables, and handles every interrupt and system call.

## Worked example: what the OS does for `cat notes.txt`

1. **The shell reads your keystrokes** - delivered by the terminal driver through `read()`.
2. **The shell creates a process** - `fork()` duplicates the shell, then
   `execve("/usr/bin/cat", ...)` replaces the child's memory with the `cat` program
   ([program vs process](<../anatomy of process/program-vs-process.md>)).
3. **The scheduler runs it** - the new process joins a run queue and is eventually given a core.
4. **`open("notes.txt")`** - the kernel resolves the path through the file system, checks
   permissions, and returns a file descriptor.
5. **`read(fd, buf, n)`** - if the data isn't in the page cache, the storage driver issues a
   DMA request and `cat` sleeps until the disk interrupt says the data has arrived.
6. **`write(1, buf, n)`** - the bytes go to stdout, i.e. your terminal.
7. **`exit(0)`** - the kernel frees the process's memory and file descriptors; the shell's
   `wait()` returns.

Every step is one of the three jobs: abstraction (files, file descriptors), resource
management (scheduling, page cache), protection (the permission check, the separate address
space).

## Clarifications

- **The kernel isn't constantly "running in the background".** It runs only when something
  enters it: a system call, an interrupt (timer, disk, NIC), or a fault (page fault, divide by
  zero). The rest of the time, user code has the CPU.
- **A system call is not a function call.** It switches the CPU into kernel mode, which is
  ~40x more expensive than a plain call - measured in
  [`syscall-cost`](../../practice/os-concepts/syscall-cost).
- **Not every computer needs a full OS.** Microcontroller firmware runs one program that owns
  the hardware; RTOSes (FreeRTOS, Zephyr) are tiny kernels for predictable timing. Once you
  have many programs, many users, or untrusted code, you need what this course describes.

## Practice project

[`practice/os-into/hello-three-ways`](../../practice/os-into/hello-three-ways) - print "hello"
via `printf`, via `write()`, and via a hand-written `syscall` instruction, to show that every
path ends at the same kernel doorway; plus `sysinfo.sh`, a tour of your machine's CPUs,
caches, memory, disks, NICs and interrupts as the kernel reports them.

## Related

- [Anatomy of a process](<../anatomy of process/anatomy-of-a-process.md>) - the next section:
  what the OS builds when it runs a program.
- [Kernel vs user mode switching](<../os concepts/kernel-vs-user-mode-switching.md>) - the
  hardware mechanism behind job 3.
- [Inside the CPU](<../inside the cpu/inside-the-cpu.md>) - the top-left box of the
  architecture diagram, opened up.
