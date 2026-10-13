# The Stack

The stack is the region of a process's memory that holds **everything tied to a function
call**: local variables, arguments that don't fit in registers, saved registers, and - most
importantly - the **return address** that says where to continue when the function finishes.
It's called a stack because it's strictly **last in, first out**: the most recently called
function is always the first to return.

## How it works

- One register, the **stack pointer** (`rsp` on x86-64, `sp` on ARM), always points at the
  current top of the stack.
- On x86-64 (and ARM, RISC-V) the stack **grows downward**: pushing something *subtracts* from
  `rsp`, popping *adds* to it.
- Each function call gets a **stack frame**: a contiguous chunk holding that call's data. By
  convention, a second register, the **base pointer** (`rbp`), marks the start of the current
  frame, so locals can be found at fixed offsets like `rbp-8`.

```text
 higher addresses
 +---------------------------+
 | main's frame              |
 |   locals of main          |
 +---------------------------+
 | return address into main  |  <- pushed by `call foo`
 | saved rbp (main's)        |  <- pushed by foo's prologue      <- rbp (foo's frame base)
 | foo's locals              |
 | ...                       |                                   <- rsp (top of stack)
 +---------------------------+
 | (free - the stack grows   |
 |  into this space)         |
 lower addresses
```

🖼️ **stack frames:**
![stack frames](<../../assets/img/stack-frames.png>)

## Why the stack is fast

- **Allocation is one instruction.** Making room for 64 bytes of locals is `sub rsp, 64`.
  Freeing them is `add rsp, 64` (or `leave`). No bookkeeping, no searching, no locks.
- **It's always hot in the cache.** The top of the stack is touched constantly, so it's
  almost always in L1.
- **It's per-thread.** Every thread has its own stack, so there's never contention.

Compare with the [heap](the-heap.md), where `malloc` has to find a free block, record its
size, and possibly ask the kernel for more memory.

## Why the stack is limited

- **Fixed maximum size.** The main thread's stack on Linux is 8MB by default (`ulimit -s`);
  other threads get a size fixed at creation (glibc defaults to the same limit; Windows uses
  1MB). Below the stack the kernel leaves a **guard gap** of unmapped pages, so running off the
  end is a segmentation fault - a **stack overflow** - instead of silently corrupting other
  memory.
- **Lifetime is tied to the call.** When a function returns, its frame is gone. Anything that
  must outlive the call has to go on the heap (or be returned by value).
- **Size must be reasonable at compile time.** A 10MB local array (`char buf[10 << 20];`)
  overflows the stack immediately.

## Common bugs

```c
int *broken(void) {
    int value = 42;
    return &value;          // value's frame is popped on return - the pointer now dangles
}
```

The memory isn't "freed" in any active sense; the stack pointer just moves back up, and the
**next function call reuses the same bytes**, overwriting `value`. The bug often looks fine in
testing, then breaks under different call patterns.

```c
void recurse(int n) { char buf[1024]; recurse(n + 1); }   // ~8000 calls -> stack overflow
```

Unbounded recursion is the classic overflow. Each call adds a frame; 8MB / ~1KB per frame ≈
8,000 calls.

**Buffer overflows** on the stack (writing past the end of a local array) can overwrite the
saved return address - the basis of decades of security exploits. Modern defenses: **stack
canaries** (`-fstack-protector`, a random value checked before `ret`), **non-executable
stacks** (NX bit), **ASLR**, and newer **shadow stacks** (Intel CET) that keep a second,
protected copy of return addresses.

## Clarifications

- **"Stack" (the region) vs "stack" (the data structure).** Same LIFO idea, but the memory
  region is managed by the CPU's push/pop/call/ret instructions, not by code you write.
- **Not all local variables are on the stack.** With optimizations on, the compiler keeps most
  locals in registers and only "spills" them to the stack when it runs out. `-O0` puts
  everything on the stack, which is why it's useful for learning.
- **`static` locals are not on the stack** - they live in the [data section](data-section.md)
  and persist across calls.
- **Arguments usually aren't on the stack either** on x86-64 Linux - the first six integer
  arguments go in registers (`rdi, rsi, rdx, rcx, r8, r9`). Only extras spill to the stack.
- **Kernel stacks are separate and tiny.** Each thread also has a kernel-mode stack (16KB on
  x86-64 Linux) used while it's executing a system call.

## Practice project

[`practice/anatomy-of-process/stack-frames`](../../practice/anatomy-of-process/stack-frames) -
`./bin/stack frames` prints frame addresses going down, `./bin/stack overflow` measures how
deep 8MB goes (~8,000 frames of ~1KB), and `./bin/stack dangling` shows a returned pointer to a
local being overwritten by the next call.

## Related

- [Process execution with stack](process-execution-with-stack.md) - exactly what `call` and
  `ret` do to the stack.
- [The heap](the-heap.md) - the alternative when a lifetime or size isn't bounded by a call.
- [Process vs thread](<../process management/process-vs-thread.md>) - every thread gets its own stack.
