# Pipelining and Parallelism

If a CPU finished one instruction completely before starting the next, most of its hardware
would sit idle most of the time - the decoder idle while the ALU works, the ALU idle while a load
waits on the cache. **Pipelining** overlaps the stages of consecutive instructions, and further
forms of **parallelism** run many instructions - or many threads - at the same time.

## Pipelining: an assembly line

Without a pipeline (one instruction at a time):

```text
cycle:   1   2   3   4   5   6   7   8   9  10
i1:      F   D   E   M   W
i2:                          F   D   E   M   W         -> 1 instruction every 5 cycles
```

With a 5-stage pipeline:

```text
cycle:   1   2   3   4   5   6   7   8   9
i1:      F   D   E   M   W
i2:          F   D   E   M   W
i3:              F   D   E   M   W
i4:                  F   D   E   M   W
i5:                      F   D   E   M   W            -> 1 instruction finishes EVERY cycle
```

Each instruction still takes 5 cycles (**latency** unchanged), but one completes every cycle
(**throughput** 5x). Real CPUs use 14-20+ stages so each stage can be simpler and the clock faster.

🖼️ **pipeline diagram:**
![pipeline diagram](<../../assets/img/pipeline.png>)

## What breaks the pipeline: hazards

### Data hazards - "I need a result that isn't ready yet"

```asm
add  rax, rbx      ; produces rax
sub  rcx, rax      ; needs rax - immediately
```

- **Forwarding (bypassing)** routes the ALU's result straight to the next instruction's input
  without waiting for write-back - so simple dependencies cost little.
- A **load** followed by its use is worse: the data isn't available until the memory stage, and
  on a cache miss the dependent instruction waits hundreds of cycles.
- Long **dependency chains** (`sum += a[i]` with one accumulator) limit how much the CPU can
  overlap, no matter how wide it is.

### Control hazards - "where do I fetch from next?"

A conditional branch isn't resolved until it executes, many stages after fetch. Instead of
waiting, the CPU **predicts** the outcome and keeps fetching speculatively.

- Modern predictors are right **>95%** of the time on typical code, using the history of
  recent branches.
- A **misprediction** throws away all the speculatively fetched work and restarts: roughly
  **15-20 cycles lost** each time on a modern x86 core.
- Data-dependent branches on random data (`if (x >= 128)` over shuffled values) are close to
  50/50 and mispredict constantly - the [branch demo](../../practice/inside-the-cpu/cpu-wait-times)
  runs 6.8x slower on shuffled data than on sorted data with identical work.

### Structural hazards - "two instructions need the same unit"

Two multiplications want the one multiplier in the same cycle; one has to wait. Cores
reduce this by duplicating units (several ALUs, two or three load ports).

## Beyond one pipeline: instruction-level parallelism (ILP)

- **Superscalar** - several pipelines side by side: decode and issue 4-8 instructions per cycle.
- **Out-of-order execution** - instructions wait in a scheduler and run as soon as their inputs
  are ready, not in program order. A **reorder buffer** (hundreds of entries) then retires them
  in order, so the program can't tell. This lets the core work on independent instructions
  while one is stuck waiting for memory.
- **Register renaming** - removes false dependencies when the same register name is reused for
  unrelated values.
- **Speculative execution** - running instructions past unresolved branches (and discarding the
  results if the guess was wrong). **Spectre** (2018) showed that discarded speculative work can
  still leave traces in the cache that leak secrets.

## Data-level parallelism: SIMD

One instruction, many values: **SSE** (128-bit, 4 floats), **AVX2** (256-bit, 8 floats),
**AVX-512** (16 floats); **NEON/SVE** on ARM. Compilers **auto-vectorize** simple loops at `-O2`/
`-O3`; libraries like BLAS, simdjson and video codecs use SIMD by hand.

## Thread-level and process-level parallelism

- **Multi-core** - truly separate instruction streams, one per core.
- **SMT / Hyper-Threading** - two instruction streams sharing one core's units, filling its idle
  slots.
- **Multiple processes / machines** - the OS's and the cluster's level.

This is where the OS comes in: it's the [scheduler](<../process management/process-management.md>)
that puts threads on cores. Hardware ILP is automatic; thread-level parallelism has to be
written into the program.

## Amdahl's law

If a fraction `s` of a program is inherently serial, the maximum speedup on `N` processors is:

```text
speedup(N) = 1 / ( s + (1 - s) / N )
```

With 10% serial work, even infinite cores give at most **10x**. That's the ceiling every parallel
design runs into - and the same reasoning applies to any pipeline of steps: speeding up one stage
only helps until another stage dominates (see how removing firmware from reboots with kexec made
the kernel's own copy/verify/decompress steps worth optimizing, in
[the ByteDance reboot case](<../os concepts/how-tiktok-bytedance-improved-linux-reboot.md>)).

## Clarifications

- **Pipelining improves throughput, not latency.** A single instruction isn't faster; more of
  them finish per second.
- **Concurrency ≠ parallelism.** Concurrency is dealing with many things at once (interleaving);
  parallelism is doing many things at the same instant. One core can be concurrent but not
  parallel (see [concurrency](<../process management/concurrency.md>)).
- **The compiler is part of the story.** Branchless code (`cmov`), loop unrolling, multiple
  accumulators and vectorization are compiler techniques for feeding the pipelines.

## Practice project

[`practice/inside-the-cpu/cpu-wait-times`](../../practice/inside-the-cpu/cpu-wait-times) -
`branch` measures misprediction cost (sorted vs shuffled data), and `ilp` measures a dependency
chain vs four independent chains (3.3x faster for the same number of additions). The Makefile
disables vectorization and if-conversion so the effects stay visible - and explains how to turn
them back on to watch the compiler fix both problems.

## Related

- [Instruction life cycle](instruction-life-cycle.md) - the stages being overlapped.
- [Inside the CPU demo: CPU wait times](inside-the-cpu-demo-cpu-wait-times.md) - where the
  pipeline ends up waiting.
- [When do you use threads?](<../process management/when-do-you-use-threads.md>) - thread-level
  parallelism in practice.
