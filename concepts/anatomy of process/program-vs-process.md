# Program vs Process

A **program** is a file. A **process** is that file brought to life: loaded into memory, given
an identity, and scheduled onto a CPU. One program can be running as many processes at once,
each with its own memory and state.

## The program: a file on disk

On Linux a program is an **ELF** file (Executable and Linkable Format); on Windows a **PE**
file (`.exe`); on macOS **Mach-O**. Inside, it's organized into **sections**:

| Section | Holds |
| --- | --- |
| `.text` | machine code |
| `.rodata` | constants, string literals |
| `.data` | global/static variables with a non-zero initial value |
| `.bss` | global/static variables that start at zero - only their *size* is stored |
| headers | entry point address, which sections to load where, required shared libraries |

```bash
file /bin/ls              # ELF 64-bit LSB pie executable, x86-64, dynamically linked...
readelf -h /bin/ls        # the header: entry point, type, machine
size /bin/ls              # text / data / bss sizes
```

A program is **passive**: it does nothing, holds no state, and uses no CPU.

## The process: a running instance

When you run the program, the kernel creates a process, which has:

- **a virtual address space**, with the program's sections mapped in, plus a stack, a heap,
  and shared libraries (see [anatomy of a process](anatomy-of-a-process.md));
- **CPU state**: the program counter (which instruction is next), the stack pointer, and all
  the other registers - saved in the kernel whenever the process isn't running;
- **kernel resources**: open file descriptors, sockets, current directory, signal handlers,
  environment variables;
- **an identity**: a PID, a parent PID, a user and group;
- **a state** in its lifecycle.

On Linux all of this hangs off one kernel structure, `struct task_struct` - the textbook name is
**process control block (PCB)**.

🖼️ **program vs process:**
![program vs process](<../../assets/img/program-vs-process.png>)

## One program, many processes

```bash
sleep 100 &  sleep 100 &
ps -o pid,cmd | grep sleep      # two PIDs, one program
```

Both processes are independent - separate memory, separate state - but the kernel is smart about
the read-only parts: the `.text` pages of `/usr/bin/sleep` (and of `libc.so`) are mapped into
both processes from the **same physical memory**. Only writable pages are per-process.

## How a process is born (Unix)

```text
 shell (pid 100)
   |  fork()        -> exact copy: pid 101, same program, same memory (copy-on-write)
   v
 child (pid 101)
   |  execve("/usr/bin/cat", ...)
   v                -> same pid 101, but memory replaced with cat's program
 cat (pid 101) ... exit(0)
   |
   v
 zombie until the shell calls wait() and collects the exit status
```

- **`fork()`** clones the calling process.
- **`execve()`** replaces the current process's program with a new one - same PID, new memory.
- **`wait()`** lets the parent collect the child's exit status; until then, the finished child
  lingers as a **zombie** (an entry in the process table, no memory).

Windows does both steps at once with `CreateProcess()`.

## Process states

```text
            admitted              scheduler picks it
   [new] ----------> [ready] ----------------------> [running] ---- exit ----> [terminated]
                       ^   ^                            |   |
                       |   +------ preempted -----------+   |
                       |          (time slice over)          | waits for I/O, a lock,
                       +------------ I/O done --------- [blocked] <------+ sleep()...
```

`ps` shows these as `R` (running or runnable), `S` (interruptible sleep - blocked), `D`
(uninterruptible sleep, usually disk I/O), `T` (stopped), `Z` (zombie).

## Clarifications

- **"Process" ≠ "application".** Chrome is one application but dozens of processes. A shell
  pipeline (`ls | grep x`) is two processes.
- **Threads live inside processes.** A process can have many threads sharing its address space;
  see [process vs thread](<../process management/process-vs-thread.md>).
- **The same program gets different addresses each run** because of ASLR (address space layout
  randomization) - a security feature, not a sign that the program changed.

## Practice project

[`practice/anatomy-of-process/process-memory-map`](../../practice/anatomy-of-process/process-memory-map) -
`make inspect` shows the *program* (section sizes in the ELF); `./bin/layout` shows the
*process* (the same sections as live memory regions in `/proc/self/maps`). The
[mini-shell](../../practice/process-management/mini-shell) project builds processes with
`fork`/`exec`/`wait` itself.

## Related

- [Simple process execution](simple-process-execution.md) - what the CPU does once the process
  is running.
- [Data section](data-section.md) - the `.data`/`.bss` sections in detail.
- [Compilers and linkers](<../os concepts/compilers-n-linkers.md>) - how the program file gets
  built in the first place.
