# Simple Process Execution

Once the kernel has loaded a program into a process, "running" it means something very
mechanical: the CPU **fetches the instruction the program counter points at, executes it, and
moves to the next one** - billions of times per second - until the process makes a system
call, gets interrupted, or exits.

## From `execve` to `main`

1. **The kernel loads the ELF**: it maps `.text`, `.rodata`, `.data` and `.bss` into the new
   address space (mapped, not copied - pages are pulled in from disk on first use).
2. **It sets up the stack**: `argc`, `argv`, the environment and an "auxiliary vector" (page
   size, entry point, where the vDSO is...) are written at the top of a fresh stack.
3. **If the program is dynamically linked**, the kernel actually starts the dynamic loader
   (`ld-linux-x86-64.so.2`) first, which maps `libc.so` and the other shared libraries.
4. **The program counter is set to the entry point**, `_start` - a tiny piece of startup code
   from the C runtime, *not* `main`.
5. `_start` calls `__libc_start_main`, which initializes libc and finally calls **`main`**.
6. When `main` returns, its return value is passed to `exit()`, which flushes stdio buffers and
   makes the `exit_group` system call.

## The CPU's view: registers and the program counter

The CPU only ever works on **registers** - a few dozen tiny, extremely fast storage slots inside
the core. The ones that matter here (x86-64 names):

| Register | Role |
| --- | --- |
| `rip` | **instruction pointer / program counter** - address of the next instruction |
| `rsp` | **stack pointer** - top of the stack |
| `rbp` | **base pointer** - base of the current stack frame (by convention) |
| `rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`-`r15` | general purpose: arithmetic, arguments, return values |
| `rflags` | condition flags set by comparisons (zero, carry, sign...) |

Executing a program is a loop the hardware runs forever:

```text
loop:
    instruction = memory[rip]        # fetch
    rip = rip + length(instruction)  # point at the next one
    decode + execute(instruction)    # may read/write registers or memory,
                                     # may overwrite rip (a jump / call / ret)
```

(The full fetch-decode-execute cycle is in
[instruction life cycle](<../inside the cpu/instruction-life-cycle.md>).)

## A worked example

```c
int main(void) {
    int a = 1;
    int b = 2;
    int c = a + b;
    return c;
}
```

Compiled with `gcc -O0 -S -masm=intel` (no optimization, so every step stays visible; the
exact stack offsets can differ between compiler versions):

```asm
main:
    push    rbp                     ; save the caller's frame base
    mov     rbp, rsp                ; start our own frame
    mov     DWORD PTR -4[rbp], 1    ; a = 1      (a lives at rbp-4, on the stack)
    mov     DWORD PTR -8[rbp], 2    ; b = 2
    mov     edx, DWORD PTR -4[rbp]  ; load a into a register
    mov     eax, DWORD PTR -8[rbp]  ; load b into a register
    add     eax, edx                ; the actual work: ONE instruction
    mov     DWORD PTR -12[rbp], eax ; store c
    mov     eax, DWORD PTR -12[rbp] ; return value goes in eax
    pop     rbp
    ret
```

What to notice:

- **The CPU can't add two memory locations directly** (on most architectures). Values are
  *loaded* into registers, operated on, and *stored* back. That's the **load/store** rhythm of
  all computation.
- **Only one instruction is the actual addition.** The rest is moving data between memory and
  registers - and memory is ~100x slower than registers. With `-O2`, the compiler keeps
  everything in registers and the whole function becomes `mov eax, 3; ret`.
- **Local variables are just offsets from `rbp`** - they live on the
  [stack](the-stack.md).

🖼️ **executing one instruction:**
![executing one instruction](<../../assets/img/simple-process-execution.png>)

## When the process stops running

The loop above continues until one of:

- **a system call** (`syscall` instruction) - the process asks the kernel to do something;
- **an interrupt** - e.g. the timer saying the time slice is over, or a device needing attention;
- **an exception** - a page fault, a divide by zero, an illegal instruction.

Each one switches the CPU into kernel mode. The kernel may then resume the same process or
[context switch](<../process management/context-switching.md>) to another one - saving this
process's registers (including `rip`) so it can continue later exactly where it left off.

## Clarifications

- **`main` is not the first instruction to run.** `_start`, the dynamic loader and libc's
  initialization all run before it; `exit()` runs after it.
- **"Executing" doesn't mean "reading the file".** The kernel maps the file and the CPU fetches
  from memory; pages of code are loaded from disk lazily, the first time `rip` reaches them.
- **Modern CPUs don't really run one instruction at a time** - they pipeline and reorder (see
  [pipelining and parallelism](<../inside the cpu/pipelining-n-parallelism.md>)) - but they
  preserve the *illusion* of this simple sequential loop for each thread.

## Practice project

[`practice/anatomy-of-process/stack-frames`](../../practice/anatomy-of-process/stack-frames) -
`make asm` prints the compiler's output for a small program; `make step` walks it in gdb,
printing `rip`, `rsp` and `rbp` at each stop.

## Related

- [The stack](the-stack.md) - where `rbp-4` actually is.
- [Process execution with stack](process-execution-with-stack.md) - the same walk-through,
  across a function call.
- [Instruction life cycle](<../inside the cpu/instruction-life-cycle.md>) - what happens
  *inside* the CPU for each instruction.
