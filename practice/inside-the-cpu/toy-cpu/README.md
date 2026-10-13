# Practice: Toy CPU

Concept notes:
[`cpu-components-n-architecture.md`](<../../../concepts/inside the cpu/cpu-components-n-architecture.md>) ·
[`instruction-life-cycle.md`](<../../../concepts/inside the cpu/instruction-life-cycle.md>) ·
[`inside-the-cpu-demo-cpu-wait-times.md`](<../../../concepts/inside the cpu/inside-the-cpu-demo-cpu-wait-times.md>)

## What this demonstrates

A CPU you can read in one sitting: **4 registers, a program counter, an instruction register,
one flat memory** holding both code and data, and an **8-instruction ISA**. Every instruction
goes through **fetch → decode → execute → memory → write-back**, printed as it happens.

It also has a cost model: ALU work takes a cycle, but a trip to memory stalls the core for
100 cycles (roughly a real RAM access). Running the same computation two ways - keeping the
running total in a **register** vs in **memory** - shows where a real CPU's time actually goes:
waiting.

## Folder structure

```text
toy-cpu/
  cpu.c       <- the machine, the ISA, two built-in programs, the cycle counter
  Makefile    <- `make`, `make demo`
```

## Build & run

```bash
make
./bin/cpu regs --trace     # total kept in a register
./bin/cpu mem --trace      # total loaded/stored from memory every iteration
./bin/cpu mem 4            # same program, but memory is as fast as an L1 cache hit
make demo                  # all three
```

## What you'll actually see

```text
program 'regs', memory latency 100 cycles
  pc=00 ir=0x01000000 | LOADI r0, 0    | write-back r0 = 0
  pc=01 ir=0x01010005 | LOADI r1, 5    | write-back r1 = 5
  pc=02 ir=0x01020001 | LOADI r2, 1    | write-back r2 = 1
  pc=03 ir=0x02000001 | ADD   r0, r1   | ALU r0 += r1 -> 5
  pc=04 ir=0x03010002 | SUB   r1, r2   | ALU r1 -= r2 -> 4
  pc=05 ir=0x06010003 | JNZ   r1, 3    | branch taken
  pc=03 ir=0x02000001 | ADD   r0, r1   | ALU r0 += r1 -> 9
  ...
  pc=06 ir=0x050000c8 | STORE r0, 200  | MEMORY write [200] = 15  (stalled 100 cycles)
  >> r0 = 15

  instructions executed : 21
  total cycles          : 163
  cycles stalled on mem : 100 (61%)
  instructions per cycle: 0.129

program 'mem', memory latency 100 cycles
  ...
  pc=04 ir=0x040000c8 | LOAD  r0, 200  | MEMORY read [200] -> r0 = 5  (stalled 100 cycles)
  pc=05 ir=0x02000001 | ADD   r0, r1   | ALU r0 += r1 -> 9
  pc=06 ir=0x050000c8 | STORE r0, 200  | MEMORY write [200] = 9  (stalled 100 cycles)
  ...
  instructions executed : 32
  total cycles          : 1296
  cycles stalled on mem : 1200 (93%)
  instructions per cycle: 0.025

program 'mem', memory latency 4 cycles
  total cycles          : 144
  cycles stalled on mem : 48 (33%)
  instructions per cycle: 0.222
```

Same answer (15), but the memory-heavy version is **8x slower**, and 93% of its cycles are
spent doing nothing. Making memory fast (4 cycles - what a cache gives you) recovers almost
all of it. That is the entire argument for registers, caches, and compilers that keep values
in registers (`-O2`) instead of spilling every local to the stack (`-O0`).

## Code walkthrough

**Encoding** - every instruction is one 32-bit word, so "decode" is just bit-slicing:

```c
//  [ opcode:8 | reg a:8 | operand b:16 ]
#define ENC(op, a, b) ((uint32_t)(op) << 24 | (uint32_t)(a) << 16 | (uint16_t)(b))

ENC(ADD, 0, 1)      // 0x02000001 -> "r0 += r1"
ENC(LOAD, 0, 200)   // 0x040000c8 -> "r0 = mem[200]"   (200 = 0xc8)
```

**The cycle** - one loop iteration per instruction:

```c
c->ir = c->mem[c->pc];        // FETCH: instruction at pc -> instruction register
uint32_t at = c->pc++;        //        pc now points at the NEXT instruction
enum opcode op = c->ir >> 24; // DECODE: split the bits
int a = (c->ir >> 16) & 0xff;
int b = c->ir & 0xffff;
switch (op) {                 // EXECUTE / MEMORY / WRITE-BACK
case ADD:  c->r[a] += c->r[b]; break;                      // ALU, 1 cycle
case LOAD: c->cycles += c->mem_latency;                    // the core just waits
           c->r[a] = (int32_t)c->mem[b]; break;
case JNZ:  if (c->r[a] != 0) c->pc = b; break;             // a branch = overwrite pc
```

- Code and data share `mem[]` - a **von Neumann** machine, like every general-purpose CPU.
  The program is "loaded" with a `memcpy` into address 0, the way an OS loader copies (well,
  maps) an executable into memory.
- A branch is nothing more than writing a new value into the program counter.
- `pc++` happens during fetch, which is why a real CPU's `call` instruction pushes the address
  of the *next* instruction as the return address.

## Try this yourself

- Add a `MUL` opcode and write a factorial program.
- Add a tiny **cache**: remember the last 4 addresses loaded; a hit costs 4 cycles, a miss
  100. Rerun `mem` - the loop reuses address 200 every iteration, so after the first miss
  it should be almost as fast as `regs`.
- Model a **pipeline**: instead of 3 cycles per instruction, charge 1 cycle per instruction
  plus a 2-cycle penalty on taken branches (the pipeline flush). Compare the cycle counts.
- Add a `CALL`/`RET` pair with a stack pointer register - and you've rebuilt the
  [stack](<../../../concepts/anatomy of process/the-stack.md>).

## Real-world equivalent

Emulators (QEMU in TCG mode, console emulators, the JVM and Python bytecode interpreters) are
this loop, scaled up: fetch an instruction, decode it, dispatch on the opcode. A real x86 core
does the same steps in hardware - but decodes into micro-ops, pipelines ~15-20 stages, executes
several instructions per cycle out of order, and hides most memory latency behind caches.
[`cpu-wait-times`](../cpu-wait-times) measures those effects on your actual CPU.
