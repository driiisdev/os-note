# Inside the CPU - Section Intro

The OS's most important job is deciding what the CPU does next. To understand the costs behind
those decisions - why context switches hurt, why cache-friendly code wins, why a thread that's
"running" can still be mostly waiting - you need a working model of what's inside a CPU core and
how it executes instructions.

## The model to build

```text
 +------------------------------- one core ----------------------------------+
 |  front end:  [branch predictor] -> [fetch] -> [decode -> micro-ops]       |
 |                      ^                 ^                                  |
 |                      |            [L1 instruction cache]                  |
 |  back end:   [rename / reorder buffer] -> [scheduler]                     |
 |                 -> [ALU][ALU][ALU][FPU/SIMD][load][load][store]           |
 |              [registers]           [L1 data cache]  [TLB]                 |
 |                                       [L2 cache]                          |
 +---------------------------------------|-----------------------------------+
                                  [shared L3 cache] --- [memory controller] --- RAM
```

- A core **fetches** instructions, **decodes** them, **executes** them on functional units,
  and **writes back** results - the [instruction life cycle](instruction-life-cycle.md).
- It overlaps many instructions at different stages - a **pipeline** - and runs several at
  once when they don't depend on each other - [pipelining and parallelism](pipelining-n-parallelism.md).
- All of it is fed from a [memory hierarchy](<../memory management/the-anatomy-of-memory.md>)
  where the next level down is 3-10x slower - so a big part of a core's design is about not
  waiting for memory, and a big part of its time is spent waiting anyway.

🖼️ **inside a core:**
![inside a core](<../../assets/img/inside-a-core.png>)

## What this section covers

| # | Topic | The question it answers |
| --- | --- | --- |
| 1 | [CPU components and architecture](cpu-components-n-architecture.md) | What are the parts of a CPU, and what do cores, threads, ISAs and clock speed actually mean? |
| 2 | [Instruction life cycle](instruction-life-cycle.md) | What happens to a single instruction from fetch to write-back? |
| 3 | [Pipelining and parallelism](pipelining-n-parallelism.md) | How does a CPU execute more than one instruction at a time - and what breaks that? |
| 4 | [Inside the CPU demo: CPU wait times](inside-the-cpu-demo-cpu-wait-times.md) | How much time does a CPU spend waiting, and on what? |

## Why the OS cares

- **Scheduling** decisions trade off throughput against cache warmth - moving a thread to
  another core throws away its cached data.
- **Context switches** are expensive partly because they flush pipelines and pollute caches and
  TLBs.
- **Interrupts** (timer, devices) are how the OS gets control back from user code - they
  arrive between instructions.
- **Privilege levels** (user vs kernel mode) are a CPU feature the OS's whole security model
  rests on.
- **Hyperthreading** means two "CPUs" in `nproc` may share one core's execution units.

## Practice projects

- [`practice/inside-the-cpu/toy-cpu`](../../practice/inside-the-cpu/toy-cpu) - an 8-instruction
  CPU with a visible fetch/decode/execute trace and a memory-stall cycle counter.
- [`practice/inside-the-cpu/cpu-wait-times`](../../practice/inside-the-cpu/cpu-wait-times) -
  cache latency ladder, branch misprediction, instruction-level parallelism on your real CPU.

## Related

- [Memory management](<../memory management/memory-management.md>) - the memory side of the
  same hardware.
- [Process management](<../process management/process-management.md>) - how the OS shares
  cores between threads.
