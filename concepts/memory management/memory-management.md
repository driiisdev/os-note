# Memory Management - Section Intro

The [previous section](<../anatomy of process/anatomy-of-a-process.md>) showed a process's
memory as a neat layout of text, data, heap and stack. This section goes underneath it: what
physical memory actually is, what it costs to read and write it, how the OS gives every process
the illusion of its own private memory (**virtual memory**), and how devices move data into
memory without the CPU's help (**DMA**).

## The problem memory management solves

Without it, every program would see raw physical RAM:

- **No isolation** - any program could read or overwrite any other program's memory, or the
  kernel's.
- **No relocation** - two programs compiled to use address `0x400000` couldn't run together.
- **Fragmentation** - a program needing 1GB contiguous couldn't start if free RAM was scattered.
- **No overcommit / swap** - total memory used by all programs could never exceed RAM.

The fix is a layer of indirection - **virtual addresses** - translated to physical ones by
hardware (the MMU) using tables the OS maintains (page tables).

```text
  process A                           physical RAM                       process B
  virtual pages                       (frames)                           virtual pages
  +--------+                          +--------+                         +--------+
  | 0x1000 |-----------------------> | frame 7 | <-----------------------| 0x1000 |   shared
  +--------+                          +--------+                         +--------+   (libc code)
  | 0x2000 |----------+               | frame 2 |                  +-----| 0x2000 |
  +--------+          |               +--------+                   |     +--------+
  | 0x3000 |  (not    +-------------> | frame 9 |                  |     | 0x3000 |  (on disk:
  +--------+  mapped)                 +--------+                   |     +--------+   swapped)
                                      | frame 4 | <----------------+
                                      +--------+
```

🖼️ **virtual to physical:**
![virtual to physical](<../../assets/img/virtual-to-physical.png>)

## What this section covers

| # | Topic | The question it answers |
| --- | --- | --- |
| 1 | [The anatomy of memory](the-anatomy-of-memory.md) | What is RAM physically, and how is it organized? |
| 2 | [Reading and writing from and to memory](reading-n-writing-from-n-to-memory.md) | What happens, step by step, when the CPU loads or stores a value? Why do caches matter? |
| 3 | [Virtual memory](virtual-memory.md) | How does each process get its own private address space? |
| 4 | [DMA](dma.md) | How do disks and NICs put data in memory without the CPU copying it? |
| 5 | [Memory section demo](memory-section-demo.md) | How to watch page faults, RSS and caches on a real system |

Closely related, in the OS concepts section: [page tables](<../os concepts/page-tables.md>) -
the data structure that makes virtual memory work, in detail.

## Key ideas to carry forward

- **Memory is slow relative to the CPU** (~100ns vs ~0.3ns per instruction), so the hardware
  hides it behind caches - and software performance depends on access patterns.
- **Every address a program uses is virtual.** Translation happens on every single memory
  access, made fast by the TLB.
- **Memory is managed in pages** (usually 4KB). Permissions, sharing, swapping and allocation
  all happen at page granularity.
- **Physical memory is assigned lazily** - on first touch, via page faults.

## Practice projects

- [`practice/memory-management/cache-locality`](../../practice/memory-management/cache-locality) -
  cache lines and access patterns.
- [`practice/memory-management/virtual-memory-lab`](../../practice/memory-management/virtual-memory-lab) -
  copy-on-write, demand paging, huge pages.
- [`practice/os-concepts/page-table-walker`](../../practice/os-concepts/page-table-walker) -
  a simulated MMU and TLB.

## Related

- [Anatomy of a process](<../anatomy of process/anatomy-of-a-process.md>) - the virtual layout
  this section explains.
- [Inside the CPU](<../inside the cpu/inside-the-cpu.md>) - caches and the MMU live in the CPU.
