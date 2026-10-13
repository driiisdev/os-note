# Inside the CPU Demo: CPU Wait Times

A CPU that's "100% busy" in `top` may be spending most of its cycles **waiting** - for memory,
for a mispredicted branch to be flushed, for the result of a previous instruction. And a CPU
that looks idle may be waiting on **I/O**. This demo puts numbers on those waits. (The exact
steps from the lecture can be added here from the `.txt`; below is a reproducible version.)

## Two different kinds of waiting

| Kind | Where it shows up | What the core is doing |
| --- | --- | --- |
| **Stalls inside the core** - cache misses, branch mispredictions, dependency chains | counted as **busy** (`%us`/`%sy`) by `top` | the thread *is* running, but retiring few instructions per cycle |
| **Waiting for I/O** - disk, network | `%wa` (**iowait**) in `top`/`vmstat`, the thread is **blocked** | nothing on that thread's behalf - the scheduler can run something else |

## 1. Waiting on memory: the latency ladder

Pointer-chasing (`p = p->next` through a random cycle) makes every load depend on the previous
one, so each step costs the full latency of wherever the data lives.

```bash
cd practice/inside-the-cpu/cpu-wait-times && make && ./bin/latency
```

```text
working set  ns / load
     16 KB       1.04  ##                    L1
    256 KB       6.09  ############          L2
      2 MB      26.65  #####################################################   L3
    256 MB     153.21  ############################################################   RAM (+ TLB misses)
```

At ~3GHz, **150ns ≈ 450 cycles** - the time the core could have run hundreds of simple
instructions. Out-of-order execution hides some of it when there's independent work; a linked
list traversal has none.

## 2. Waiting on the pipeline: branch misprediction

```bash
./bin/branch
```

```text
shuffled data:   244.0 ms
sorted data  :    36.1 ms   (identical work)
mispredictions made the shuffled run 6.8x slower
```

## 3. Waiting on a previous result: dependency chains

```bash
./bin/ilp
```

```text
1 accumulator :   187.2 ms
4 accumulators:    56.1 ms
same number of additions, 3.3x faster when they're independent
```

## 4. Waiting on I/O: iowait

```bash
vmstat 1        # watch the 'wa' column
top             # '%wa' in the CPU line
# in another terminal, generate disk I/O, e.g.:
dd if=/dev/zero of=~/big.bin bs=1M count=2048 oflag=direct; rm ~/big.bin
```

`wa` rises while threads are blocked in the kernel waiting for the device. That CPU time isn't
lost to the *system* - the scheduler runs other threads - but the waiting threads make no
progress.

**`%st` (steal)** is the virtualization version: time your VM wanted to run but the hypervisor
gave the physical CPU to someone else. Visible in WSL2 and cloud VMs.

🖼️ **where cycles go:**
![where cycles go](<../../assets/img/cpu-wait-times.png>)

## Measuring it properly (bare-metal Linux)

`perf` reads the CPU's hardware performance counters:

```bash
perf stat -e cycles,instructions,cache-misses,branch-misses ./bin/latency
#   instructions per cycle (IPC) far below 1  ->  memory-bound
perf stat -e cycles,instructions,branch-misses ./bin/branch
```

`perf` doesn't work in stock WSL2 (no access to hardware counters through Hyper-V), which is why
the practice projects use wall-clock timing instead.

## What to take away

- **"CPU-bound" often means "memory-bound".** If IPC is low, the fix is data layout and access
  pattern, not a faster algorithm on paper.
- **Unpredictable branches cost real time** - sorting, branchless code or SIMD can remove them.
- **Independent work is free parallelism** that the core finds for you, if the code exposes it.
- **iowait is a different problem** - solved with async I/O, more threads, or faster storage,
  not with CPU optimizations.

## Practice projects

- [`practice/inside-the-cpu/cpu-wait-times`](../../practice/inside-the-cpu/cpu-wait-times) -
  experiments 1-3.
- [`practice/inside-the-cpu/toy-cpu`](../../practice/inside-the-cpu/toy-cpu) - the same idea in
  a simulator: 93% of cycles stalled on memory when a value lives in RAM instead of a register.
- [`practice/storage-management/file-io-journey`](../../practice/storage-management/file-io-journey) -
  generates the kind of I/O that shows up as `%wa`.

## Related

- [Reading and writing from and to memory](<../memory management/reading-n-writing-from-n-to-memory.md>) -
  the path behind experiment 1.
- [Pipelining and parallelism](pipelining-n-parallelism.md) - the theory behind experiments 2 and 3.
- [When do you use threads?](<../process management/when-do-you-use-threads.md>) - the I/O-wait
  side, from the program's point of view.
