# Anatomy of a Process - Section Intro

A **process** is the OS's answer to "a program is running". This section opens one up: what it
is compared to the program file it came from, how the CPU actually executes its instructions,
and the memory regions every process is made of - **text, data, heap and stack** - and why
each one exists.

## The big picture

When you run a program, the kernel builds a process around it:

```text
 high addresses  +---------------------------+
                 |  kernel space             |  (mapped, but inaccessible from user mode)
 0x7fff...       +---------------------------+
                 |  stack       (grows down) |  function frames: locals, return addresses
                 |      |                    |
                 |      v                    |
                 |                           |
                 |  mmap region              |  shared libraries, big mallocs, mapped files
                 |                           |
                 |      ^                    |
                 |      |                    |
                 |  heap        (grows up)   |  malloc/new: dynamic, lifetime you control
                 +---------------------------+
                 |  bss    zero-initialized globals          |
                 |  data   initialized globals / statics     |  fixed size, known at
                 |  rodata constants, string literals        |  compile time
                 |  text   machine code                      |
 low addresses   +-------------------------------------------+
```

Along with that address space, the kernel keeps bookkeeping for each process in a **process
control block** (Linux: `struct task_struct`): its PID, state, saved CPU registers, open files,
memory map, scheduling information, parent and children.

🖼️ **process memory layout:**
![process memory layout](<../../assets/img/process-memory-layout.png>)

## What this section covers

| # | Topic | The question it answers |
| --- | --- | --- |
| 1 | [Program vs process](program-vs-process.md) | What's the difference between `/usr/bin/cat` and a running `cat`? |
| 2 | [Simple process execution](simple-process-execution.md) | How does the CPU run a program's instructions, one by one? |
| 3 | [The stack](the-stack.md) | Where do local variables and function calls live? |
| 4 | [Process execution with stack](process-execution-with-stack.md) | What exactly happens in memory on a function call and return? |
| 5 | [Data section](data-section.md) | Where do global and static variables live? |
| 6 | [The heap](the-heap.md) | Where does `malloc` memory come from, and why is it slower? |
| 7 | [Process section demo](process-section-demo.md) | How to see all of this on a real process |

## Why each region exists

The regions differ in **when their size is known** and **how long their contents live**:

| Region | Size known | Lifetime | Managed by |
| --- | --- | --- | --- |
| text, rodata | compile time | whole program | loader (read-only) |
| data, bss | compile time | whole program | loader |
| stack | grows/shrinks per call | until the function returns | the CPU's stack pointer - automatic |
| heap | runtime | until you free it | you (or a garbage collector), via the allocator |

That table is the whole design: things whose size and lifetime are known up front go in fixed
regions; things tied to a function call go on the stack, where allocation is just moving a
pointer; everything else needs the heap.

## Clarifications

- **A process is not just memory.** It's memory + CPU state (registers, program counter) +
  kernel resources (file descriptors, sockets, signal handlers) + an identity (PID, user).
- **Every process has its own copy of this layout**, at the same-looking addresses - because
  every address here is *virtual*. That's the bridge to the
  [memory management](<../memory management/memory-management.md>) section.

## Practice projects

- [`practice/anatomy-of-process/process-memory-map`](../../practice/anatomy-of-process/process-memory-map) -
  print the address of a variable from every region and match it against `/proc/self/maps`.
- [`practice/anatomy-of-process/stack-frames`](../../practice/anatomy-of-process/stack-frames) -
  watch frames being built, read the compiler's assembly, overflow the stack on purpose.

## Related

- [OS intro](<../os into/os-intro.md>) - why the OS builds processes at all.
- [Process management](<../process management/process-management.md>) - how the OS runs many
  of these at once.
