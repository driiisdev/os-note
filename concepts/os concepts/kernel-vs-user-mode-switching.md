# Kernel vs User Mode Switching

The OS can only protect processes from each other - and the hardware from all of them - because
the **CPU itself** enforces two levels of privilege. User programs run in **user mode**, where
dangerous instructions are forbidden. The kernel runs in **kernel mode**, where everything is
allowed. The only ways from one to the other are a few controlled gates, and crossing them has a
cost.

## Privilege levels

| Architecture | Levels | Used by Linux as |
| --- | --- | --- |
| x86-64 | rings 0-3 | ring 0 = kernel, ring 3 = user (rings 1-2 unused) |
| ARM64 | EL0-EL3 | EL0 = user, EL1 = kernel, EL2 = hypervisor, EL3 = secure firmware |
| RISC-V | U / S / M | U = user, S = supervisor (kernel), M = machine (firmware) |

In user mode, the CPU refuses to:

- execute **privileged instructions** - load page tables (`mov cr3`), disable interrupts
  (`cli`), halt the CPU, configure devices via I/O ports or MSRs;
- touch memory whose page-table entry is marked **supervisor-only** - which is how the kernel's
  memory, mapped into every process, stays out of reach.

Trying either raises an exception, and the kernel kills the process (usually with SIGSEGV or
SIGILL).

## The three ways into the kernel

| Way | Triggered by | Examples |
| --- | --- | --- |
| **System call** | the program, deliberately | `read`, `write`, `open`, `mmap`, `fork` - via the `syscall` instruction (x86-64), `svc` (ARM64), `ecall` (RISC-V) |
| **Exception** (trap/fault) | the instruction just executed | page fault, divide by zero, invalid opcode, breakpoint |
| **Interrupt** | hardware, asynchronously | timer tick, disk completion, network packet arrival |

## Anatomy of a system call (x86-64 Linux)

```text
 user mode                                   kernel mode
 ---------                                   -----------
 rax = 1 (SYS_write)
 rdi = fd, rsi = buf, rdx = len
 syscall  ───────────────────────────────>   CPU: save user rip -> rcx, rflags -> r11,
                                                  switch to ring 0, jump to the kernel's
                                                  entry point (address in the LSTAR MSR)
                                             kernel: switch to the thread's kernel stack,
                                                  save user registers,
                                                  (Meltdown mitigation: switch page tables)
                                                  look up sys_call_table[rax] -> ksys_write()
                                                  validate fd, copy_from_user(buf) ...
                                                  put the result (or -errno) in rax
                                                  restore registers
          <───────────────────────────────   sysret: back to ring 3 at rip = rcx
 rax = bytes written, or -1 with errno set by the libc wrapper
```

The kernel never trusts pointers from user space: it validates them and copies data across with
`copy_from_user` / `copy_to_user`, so a bad pointer gives `EFAULT` instead of a kernel crash.

🖼️ **Image to add - syscall round trip:** a two-lane diagram (user mode lane above, kernel mode lane
below) with a call dropping from `write()` in libc through the `syscall` instruction, into the
kernel entry, the syscall table and `ksys_write`, and returning via `sysret` - with the registers
used at each step listed alongside.
![syscall round trip](<../../assets/img/syscall-round-trip.png>)

## What crossing costs

- A plain function call: ~1-2ns.
- A trivial syscall (`getpid`): ~50-100ns on modern hardware - measured at ~60ns in the practice
  project, ~44x a function call.
- Since 2018, **Meltdown/Spectre mitigations** (KPTI - kernel page-table isolation, IBRS, retpolines)
  made the crossing noticeably more expensive on affected CPUs; KPTI switches page tables on every
  entry and exit.
- Plus indirect costs: kernel code and data competing with the program for cache and TLB space.

## Avoiding the crossing

- **vDSO** - the kernel maps a tiny shared library into every process, with read-only data it
  keeps updated. `clock_gettime()` and `gettimeofday()` run entirely in user mode through it
  (~18ns in the practice project vs ~60ns for `getpid`).
- **Batching** - stdio buffering (one `write` per buffer instead of per `printf`), `writev`,
  `sendmmsg`/`recvmmsg`, `epoll_wait` returning many events at once.
- **io_uring** - submit and complete many I/O operations through shared-memory rings with one
  syscall, or none in polling mode.
- **Kernel bypass** - DPDK (networking) and SPDK (storage) drive devices directly from user space,
  polling instead of taking interrupts.
- **eBPF** - the opposite direction: run small, verified programs *inside* the kernel to avoid
  copying data out to user space.

## Mode switch vs context switch

A **mode switch** is user → kernel → user within the *same* thread. A **context switch** changes
*which thread* runs. A syscall is always a mode switch; it becomes a context switch only if the
thread blocks (or the scheduler preempts it) before returning. See
[context switching](<../process management/context-switching.md>).

## Clarifications

- **The kernel isn't a separate process** sitting somewhere waiting for calls. It's code that
  runs *on the calling thread*, in kernel mode, using that thread's kernel stack.
- **libc functions aren't syscalls.** `printf`, `malloc`, `strlen` mostly run in user mode;
  `printf` eventually calls `write`, `malloc` occasionally calls `brk`/`mmap`.
- **Root is not kernel mode.** A root process still runs in ring 3 - it's just allowed by the
  kernel to make more syscalls succeed (permission checks), not to execute privileged
  instructions itself.

## Practice projects

- [`practice/os-concepts/syscall-cost`](../../practice/os-concepts/syscall-cost) - a function
  call vs `getpid()` vs vDSO `clock_gettime()` vs `write()` to `/dev/null`, in nanoseconds.
- [`practice/os-into/hello-three-ways`](../../practice/os-into/hello-three-ways) - making a `write`
  syscall by hand with the `syscall` instruction and inline assembly.

## Related

- [OS intro](<../os into/os-intro.md>) - the protection job this mechanism exists for.
- [Context switching](<../process management/context-switching.md>) - what happens when the
  kernel decides not to return to the same thread.
- [Asynchronous I/O](<../socket management/asynchronous-io.md>) - io_uring and batching.
- [Virtualization and containerization](virtualization-n-containerization.md) - a third level:
  the hypervisor.
