# CPU Components and Architecture

A CPU is a machine that repeatedly fetches an instruction from memory, works out what it means,
does it, and moves on. Every part of a modern CPU exists either to **do** that work or to **keep
the work fed** - because the slowest part of computing is almost never the arithmetic.

## The core components

| Component | Job |
| --- | --- |
| **Registers** | a few dozen tiny, ultra-fast storage slots the instructions operate on directly: general-purpose (`rax`, `rbx`...), the **program counter** (`rip`), the **stack pointer** (`rsp`), flags (`rflags`), plus vector registers (`xmm`/`ymm`/`zmm`) |
| **Control unit** | fetches and decodes instructions, then directs the rest of the core - in modern CPUs this is the "front end" plus the scheduling logic |
| **ALU** (arithmetic logic unit) | integer arithmetic, comparisons, bitwise logic. Modern cores have several |
| **FPU / SIMD units** | floating point and vector math - one instruction on 4, 8 or 16 values at once (SSE, AVX, AVX-512 on x86; NEON/SVE on ARM) |
| **Load/store units** | move data between registers and the L1 data cache |
| **Caches** | L1 instruction + L1 data (per core), L2 (per core), L3 (shared) - SRAM that hides DRAM latency |
| **MMU + TLB** | translate virtual addresses to physical on every access (see [virtual memory](<../memory management/virtual-memory.md>)) |
| **Branch predictor** | guesses which way branches go so the front end can keep fetching without waiting |
| **Clock** | the beat that advances everything; GHz = billions of cycles per second |

```text
  +----------------------------- CPU package ---------------------------------+
  |  +--------- core 0 ----------+   +--------- core 1 ----------+            |
  |  | registers, ALUs, FPU/SIMD |   | registers, ALUs, FPU/SIMD |   ...      |
  |  | control / branch predict  |   | control / branch predict  |            |
  |  | L1i 32KB   L1d 32-48KB    |   | L1i 32KB   L1d 32-48KB    |            |
  |  | L2 256KB - 2MB, TLBs, MMU |   | L2 256KB - 2MB, TLBs, MMU |            |
  |  +---------------------------+   +---------------------------+            |
  |  [=============== L3: several MB, shared by all cores =================]  |
  |  [ memory controller ]   [ PCIe controller ]   [ integrated GPU (some) ]  |
  +---------------------------------------------------------------------------+
```

🖼️ **CPU package:**
![CPU package](<../../assets/img/cpu-package.png>)

## Cores, hardware threads, sockets

- **Core** - a complete execution engine: its own registers, pipelines, L1/L2. N cores = N
  instruction streams truly running at the same time.
- **Hardware thread (SMT / Hyper-Threading)** - one core presenting itself as two logical
  CPUs, each with its own registers, sharing the core's execution units and caches. When one
  thread stalls (e.g. on a cache miss), the other uses the idle units. Typical gain: 10-30%,
  not 2x.
- **Socket** - a physical CPU package. Multi-socket servers have NUMA memory: each socket has
  its own local RAM.

```bash
lscpu                                    # sockets, cores per socket, threads per core, caches
nproc                                    # logical CPUs the OS can schedule on
cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list   # which logical CPUs share a core
```

The OS scheduler sees **logical CPUs**, but a good scheduler knows which ones share a core or
a cache, and places threads accordingly.

## Instruction set architecture (ISA)

The ISA is the contract between software and hardware: which instructions exist, which
registers, how memory is addressed, how privilege levels work.

| | **x86-64** (Intel, AMD) | **ARM64 / AArch64** (Apple M-series, AWS Graviton, phones) | **RISC-V** |
| --- | --- | --- | --- |
| Style | CISC: variable-length instructions (1-15 bytes), instructions can operate on memory | RISC: fixed 32-bit instructions, load/store architecture | RISC, open and royalty-free |
| General registers | 16 | 31 | 31 |
| Inside | decodes into RISC-like **micro-ops** | executes mostly directly | executes mostly directly |

In practice the CISC/RISC distinction matters less than it used to - modern x86 cores translate
instructions into internal micro-ops and run them on a RISC-like out-of-order engine. A program
compiled for one ISA can't run on another without recompiling or emulating (Apple's Rosetta 2
translates x86-64 to ARM64).

## Clock speed, IPC and performance

```text
 performance  ≈  clock frequency (cycles/second)  ×  IPC (instructions per cycle)
```

- **Clock speed** stopped climbing around 2005 (~3-5GHz) because power and heat grow steeply
  with frequency.
- **IPC** is where the gains have come from since: wider cores, deeper out-of-order windows,
  better branch prediction, bigger caches. A modern core can retire 4-8 instructions per cycle
  at its peak.
- Real code often achieves an IPC **below 1**, because it's waiting on memory or recovering from
  mispredicted branches - the theme of the [CPU wait times demo](inside-the-cpu-demo-cpu-wait-times.md).
- **Turbo / boost** - cores run faster when few are busy and the package is cool, so
  single-threaded and multi-threaded speeds differ.

## Privilege levels

The CPU enforces **modes**: x86 has rings 0-3 (Linux uses ring 0 for the kernel and ring 3 for
user programs); ARM has exception levels EL0 (user) to EL3 (firmware). Privileged instructions
(changing page tables, talking to devices, disabling interrupts) fault in user mode. This is the
hardware foundation of [kernel vs user mode](<../os concepts/kernel-vs-user-mode-switching.md>).

## Clarifications

- **More cores ≠ faster single programs.** A single-threaded program uses one core no matter
  how many you have.
- **"4 CPUs" in `nproc` may be 2 cores.** Check `Thread(s) per core` in `lscpu`.
- **GHz isn't comparable across architectures** - IPC differs, so a 3.2GHz Apple M-series core
  can outrun a 4.5GHz x86 core on many tasks.

## Practice projects

- [`practice/inside-the-cpu/toy-cpu`](../../practice/inside-the-cpu/toy-cpu) - registers, a
  program counter, an instruction register, an ALU and memory, in ~150 lines of C.
- [`practice/os-into/hello-three-ways`](../../practice/os-into/hello-three-ways) - `sysinfo.sh`
  prints your cores, threads and cache sizes.

## Related

- [Instruction life cycle](instruction-life-cycle.md) - what the components do for each instruction.
- [Pipelining and parallelism](pipelining-n-parallelism.md) - how cores overlap instructions.
- [The anatomy of memory](<../memory management/the-anatomy-of-memory.md>) - what's behind the caches.
