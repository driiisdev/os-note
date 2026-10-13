# Instruction Life Cycle

Every instruction a CPU runs - an `add`, a `load`, a `call` - goes through the same basic
sequence of steps. The classic textbook model has five stages: **fetch, decode, execute,
memory access, write-back**. Real CPUs split these into many more, smaller stages, but the five
are the right mental model.

## The five stages

```text
        +-------+    +--------+    +---------+    +--------+    +------------+
  rip ->| FETCH |--->| DECODE |--->| EXECUTE |--->| MEMORY |--->| WRITE-BACK |
        +-------+    +--------+    +---------+    +--------+    +------------+
        read bytes   what is it?   ALU does the   load/store   result into
        at [rip]     which regs?   math, or        talk to     the destination
        rip += len   read operands compute an      L1 cache    register
                                   address
```

### 1. Fetch

- Read the instruction bytes at the address in the **program counter** (`rip`) from the
  **L1 instruction cache**.
- Advance `rip` to the next instruction (on x86 that means first working out how long this
  instruction is - anything from 1 to 15 bytes).
- In a real core, the **branch predictor** decides *now* where the instruction after this one
  will come from - before the current instruction has even been decoded.

### 2. Decode

- Work out what the instruction is and which registers/operands it uses.
- On x86, translate it into one or more simpler internal **micro-ops** (μops). A complex
  instruction like `add [rbx], eax` (add a register to memory) becomes load + add + store.
- Read source register values (or mark them as "waiting" if an earlier instruction hasn't
  produced them yet).

### 3. Execute

- The **ALU** (or FPU/SIMD unit) does the actual work: add, compare, shift, multiply...
- For a load or store, this is where the memory **address is computed** (e.g. `rbx + 8*rcx`).
- For a branch, this is where the CPU learns whether it's taken - and whether the predictor
  guessed right.

### 4. Memory access

- Only for loads and stores: read from or write to the **L1 data cache** (going further down the
  hierarchy on a miss - see [reading and writing memory](<../memory management/reading-n-writing-from-n-to-memory.md>)).
- Address translation through the TLB happens here too.
- This is the stage that can stall for **hundreds of cycles** on a cache miss.

### 5. Write-back

- Write the result into the destination register, where later instructions can use it.
- In out-of-order CPUs, results are additionally **retired (committed) in program order**, so
  that if anything goes wrong (an exception, a mispredicted branch), the visible state looks as
  if instructions ran one at a time.

🖼️ **five-stage cpu cycle:**
![instruction life cycle](<../../assets/img/instruction-life-cycle.png>)

## Two instructions, traced

**`add eax, edx`** (register + register):

| Stage | What happens |
| --- | --- |
| fetch | bytes `01 d0` read from `[rip]`; `rip += 2` |
| decode | "integer add, sources eax and edx, destination eax" |
| execute | ALU computes eax + edx, sets flags (zero, carry, sign, overflow) |
| memory | nothing to do |
| write-back | result into eax |

**`mov eax, DWORD PTR [rbp-8]`** (load from the stack):

| Stage | What happens |
| --- | --- |
| fetch | bytes read, `rip` advances |
| decode | "load 4 bytes, address = rbp - 8, destination eax" |
| execute | address unit computes rbp - 8 |
| memory | TLB translates the address; L1 lookup - **hit: ~4 cycles, miss: up to hundreds** |
| write-back | loaded value into eax |

The `add` takes about one cycle of real work. The load takes 4 cycles when lucky - and that's
why, in the [toy CPU](../../practice/inside-the-cpu/toy-cpu), the program that keeps its running
total in memory spends 93% of its cycles stalled.

## Branches and calls

A branch is just an instruction whose write-back target is **`rip` itself**:

- `jmp target` - unconditionally set `rip = target`.
- `jne target` - set `rip = target` only if the zero flag (set by a previous `cmp`) is clear.
- `call f` - push the return address, then `rip = f`. `ret` - pop into `rip`.

Because the fetch stage needs to know the next `rip` *immediately*, a branch whose outcome isn't
known until the execute stage would stall the front end - which is why branch prediction exists
(see [pipelining and parallelism](pipelining-n-parallelism.md)).

## Interrupts and exceptions fit between instructions

The CPU checks for pending **interrupts** between instructions. If one is pending (timer, disk,
network), it saves `rip` and flags, switches to kernel mode, and jumps to the interrupt handler.
**Exceptions** (page fault, divide by zero, invalid opcode) are raised *by* an instruction during
its own life cycle - and, for faults like a page fault, that instruction is **restarted** from
scratch once the kernel has fixed things up.

## Clarifications

- **The five stages are a model, not a spec.** Modern x86 cores have ~14-20 pipeline stages and
  dozens of instructions in flight; the stages above are still the logical steps each one takes.
- **Fetching and executing aren't the same as reading the program file** - instructions come
  from memory (via the L1i cache), which is filled from the page cache, which was filled from disk.
- **Each instruction's cycle count isn't fixed.** It depends on where its data is, whether its
  inputs are ready, and what else is competing for the same execution unit.

## Practice project

[`practice/inside-the-cpu/toy-cpu`](../../practice/inside-the-cpu/toy-cpu) - every instruction
of a small program printed with its fetch (`pc`, `ir`), decode (opcode and operands) and
execute/memory/write-back step, with memory accesses charged 100 cycles of stall.

## Related

- [CPU components and architecture](cpu-components-n-architecture.md) - the hardware each stage uses.
- [Pipelining and parallelism](pipelining-n-parallelism.md) - running the stages for many
  instructions at once.
- [Simple process execution](<../anatomy of process/simple-process-execution.md>) - the same
  cycle from the process's point of view.
