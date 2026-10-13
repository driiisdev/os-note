# Practice: Context Switch Cost

Concept note: [`context-switching.md`](<../../../concepts/process management/context-switching.md>)

## What this demonstrates

What it costs for the OS to stop one process and start another. Two processes bounce a single
byte back and forth over a pair of pipes. Each side blocks in `read()` until the other writes,
so **every hop forces the scheduler to switch** from one process to the other.

A baseline run of the same `write()` + `read()` syscalls inside a single process (no switch
needed) separates the cost of the syscalls from the cost of the switch.

## Folder structure

```text
context-switch-cost/
  pingpong.c   <- the benchmark (pinned to one CPU, or free to use any)
  Makefile     <- `make`, `make demo`
```

## Build & run

```bash
make
./bin/pingpong        # both processes pinned to CPU 0 -> true context switches on one core
./bin/pingpong any    # free placement -> cross-core wake-ups instead
```

## What you'll actually see

```text
mode: both processes pinned to CPU 0
  write+read in one process (no switch):   0.40 us
  ping-pong round trip                  :   3.10 us  (2 hops)
  -> each hop costs ~1.35 us on top of the syscalls themselves
  this process: 100000 voluntary, 0 involuntary context switches during the run

mode: free to use any CPU
  write+read in one process (no switch):   0.45 us
  ping-pong round trip                  :  75.63 us  (2 hops)
  -> each hop costs ~37.59 us on top of the syscalls themselves
  this process: 99796 voluntary, 1 involuntary context switches during the run
```

- **~1.4us per switch on one core.** That covers saving registers, picking the next task,
  switching page tables (`CR3`), restoring registers - plus the indirect cost of cold caches.
- **100,000 voluntary switches** for 100,000 round trips - "voluntary" means the process
  blocked by itself (in `read()`), as opposed to being preempted by the timer.
- **Spread across cores it got *slower*, not faster.** Each wake-up now has to signal another
  core (an inter-processor interrupt), and that core may be in a power-saving idle state it
  must wake from. Under WSL2 every one of those goes through the Hyper-V hypervisor, which
  makes it especially expensive here; on bare metal the gap is smaller, but cross-core
  ping-pong is rarely faster than same-core for this kind of tiny message.

## Code walkthrough

```c
static void pin_to_cpu0(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    sched_setaffinity(0, sizeof set, &set);   // inherited by the child through fork()
}

// parent                                    // child
write(ping[1], &b, 1);  // wake the child    read(ping[0], &b, 1);   // blocks until woken
read(pong[0], &b, 1);   // block -> switch   write(pong[1], &b, 1);  // wake the parent
```

The switch counters come straight from the kernel:

```c
sscanf(line, "voluntary_ctxt_switches: %ld", vol);      // from /proc/self/status
sscanf(line, "nonvoluntary_ctxt_switches: %ld", invol);
```

## Try this yourself

- `vmstat 1` in another terminal while `./bin/pingpong` runs - watch the `cs` (context
  switches per second) column jump to hundreds of thousands.
- Convert it to two **threads** of one process instead of two processes. Thread switches
  don't change the address space, so they skip the page-table switch - is it measurably cheaper?
- `cat /proc/<pid>/sched` for a running process shows per-task scheduler statistics, including
  `nr_switches`.
- `perf bench sched pipe` (if you have perf) is the standard tool version of this exact test.

## Real-world equivalent

This is the cost behind the **"too many threads"** problem: a server with 10,000 threads that
each wake for a tiny bit of work spends a large fraction of its CPU on switching. It's why
event loops (nginx, Node.js, Redis) handle many connections on few threads, why Go multiplexes
goroutines onto a handful of OS threads, and why latency-sensitive systems (trading, DPDK
networking) **pin** threads to cores and busy-poll instead of blocking.
