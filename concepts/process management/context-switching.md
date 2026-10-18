# Context Switching

A core can only run one thread at a time. To give hundreds of threads the illusion of running
simultaneously, the OS keeps **pausing one and resuming another** - a **context switch**. Each
switch saves everything the paused thread needs to continue later, and restores everything the
next thread was in the middle of.

## What triggers a switch

| Trigger | Type | Example |
| --- | --- | --- |
| the thread **blocks** | voluntary | `read()` on a socket with no data, waiting for a mutex, `sleep()` |
| the thread **yields** | voluntary | `sched_yield()` |
| the **time slice** expires | involuntary (preemption) | the timer interrupt fires and the scheduler decides someone else should run |
| a **higher-priority** thread wakes | involuntary | a real-time thread becomes runnable |
| the thread **exits** | - | `exit()` |

Every switch starts with the CPU entering the kernel (a syscall, an interrupt or an exception) -
user code can't switch threads by itself.

## What gets saved and restored

```text
 thread A running                    kernel                         thread B resumes
 ---------------->  trap / interrupt  ------------------------------>  ------------>
                    1. save A's registers (rip, rsp, general purpose, flags)
                       onto A's kernel stack / into its task_struct
                    2. save A's FPU/SIMD state (xmm/ymm/zmm: up to a few KB) - often lazily
                    3. scheduler picks B
                    4. if B is in a DIFFERENT process: switch page tables (load CR3)
                    5. switch kernel stack pointer to B's
                    6. restore B's registers, FPU state
                    7. return to user mode at B's saved rip
```

🖼️ **context switch timeline:**
![context switch](<../../assets/img/context-switch.png>)

## What it costs

**Direct costs** - the work above: entering the kernel, saving/restoring registers, running the
scheduler, maybe switching page tables. Typically **~1-3 µs** on modern hardware (measured at
~1.4µs per hop in the practice project, on one core).

**Indirect costs** - often bigger, and invisible in microbenchmarks:

- **Cache pollution** - thread B's data evicts thread A's from L1/L2; when A comes back, it
  starts cold and runs slowly until its working set is reloaded.
- **TLB flush** - switching address spaces invalidates translations. CPUs with **PCID** (x86)
  or **ASID** (ARM) tag TLB entries per address space to avoid a full flush.
- **Pipeline flush** - all in-flight speculative work is discarded.
- **Security mitigations** - since Spectre/Meltdown, kernels may flush branch predictors or
  switch page tables on each kernel entry, adding to every switch.

## Thread switch vs process switch

| | Thread → thread (same process) | Process → process |
| --- | --- | --- |
| registers, stack pointer | saved / restored | saved / restored |
| page tables (`CR3`) | **unchanged** | switched |
| TLB | stays valid | flushed, or tagged with PCID |
| caches | data may still be relevant (shared heap) | other process's data - mostly useless |

That's one reason threads are preferred over processes for work that switches often.

## Mode switch ≠ context switch

A **system call** switches the CPU from user mode to kernel mode and back - but usually **within
the same thread**. That's a *mode switch* (~50-100ns), not a context switch. A context switch
happens only when the kernel decides to run a *different* thread before returning. See
[kernel vs user mode switching](<../os concepts/kernel-vs-user-mode-switching.md>).

## Seeing it on a real system

```bash
vmstat 1                                  # 'cs' column: context switches per second, system-wide
grep ctxt_switches /proc/<pid>/status     # voluntary_ctxt_switches / nonvoluntary_ctxt_switches
pidstat -w 1                              # per-process switch rates (sysstat package)
perf stat -e context-switches,cpu-migrations ./program   # bare-metal Linux
```

- Lots of **voluntary** switches → the thread blocks often (I/O, locks): normal for servers.
- Lots of **nonvoluntary** switches → threads are being preempted: more runnable threads than
  cores, i.e. CPU contention.

## Clarifications

- **Context switches aren't bad - excessive ones are.** They're how the OS shares the CPU at all.
  The problem is thousands of threads each running for microseconds between switches.
- **Going multi-core doesn't remove switching costs** - waking a thread on *another* core needs
  an inter-processor interrupt, which can be slower than a same-core switch (very visible in
  VMs, as the practice project shows).
- **Time slices are short** - a few milliseconds under Linux's fair scheduler - so even a fully
  CPU-bound thread gets preempted hundreds of times per second when it shares a core.

## Practice project

[`practice/process-management/context-switch-cost`](../../practice/process-management/context-switch-cost) -
two processes ping-pong a byte over pipes 100,000 times: ~1.4µs per switch when pinned to one
core, 100,000 voluntary switches counted by the kernel, and a much slower cross-core mode that
shows the cost of wake-up interrupts under a hypervisor.

## Related

- [Process vs thread](process-vs-thread.md) - why thread switches are cheaper.
- [When do you use threads?](when-do-you-use-threads.md) - too many threads = too many switches.
- [Socket programming patterns](<../socket management/socket-programming-patterns.md>) - event
  loops exist largely to avoid per-connection thread switching.
