# Reading and Writing from and to Memory

A single line like `x = arr[i];` triggers a surprisingly long journey: address translation,
up to three cache lookups, maybe a trip to DRAM, and the result sliding back into a register.
Writes take a similar path in reverse - except they usually *don't* wait for memory at all.
Knowing that path explains most of what makes code fast or slow.

## The journey of a load

```text
 mov eax, [rbx]       ; "load the 4 bytes at the virtual address in rbx"

  1. TLB lookup        virtual page -> physical frame        (hit: ~0-1 cycle)
       miss? -> page walk through the page tables (4 more memory reads)
       not mapped? -> PAGE FAULT -> the kernel steps in
  2. L1 data cache     ~4 cycles     hit? done
  3. L2 cache          ~12 cycles    hit? copy line into L1, done
  4. L3 cache          ~40 cycles    hit? copy line into L2 + L1, done
  5. DRAM              ~200-400 cycles
       memory controller -> open row -> burst 64 bytes back
       the whole 64-byte LINE is installed in L3, L2 and L1
  6. the 4 requested bytes go into eax
```

🖼️ **the path of a load:**
![load path](<../../assets/img/memory-load-path.png>)

Key consequences:

- **The unit of transfer is the cache line (64 bytes)**, so after one miss, the next 15 ints in
  the same line are free. This is **spatial locality**.
- **Recently used lines stay in cache**, so touching the same data again soon is cheap. This is
  **temporal locality**.
- **Hardware prefetchers** watch for sequential or strided access patterns and fetch lines
  *before* you ask for them - which is why a sequential sweep over a huge array runs close to
  memory bandwidth, while random access over the same array is ~10-50x slower.

## The journey of a store

```text
 mov [rbx], eax       ; "store 4 bytes to the virtual address in rbx"
```

1. Address translation, same as a load (plus a permission check: is the page writable?).
2. The store goes into the core's **store buffer** and the instruction **retires immediately** -
   the CPU doesn't wait.
3. In the background, the cache line is brought into L1 in an exclusive state (fetching it if
   needed - a store to a line not in cache still requires a read first, "read-for-ownership").
4. The 4 bytes are merged into the L1 line, which is now **dirty**.
5. The dirty line is only written back to L2/L3/DRAM when it's **evicted** - this is a
   **write-back** cache. Many writes to the same line cost one eventual write to RAM.

## Multiple cores: cache coherence

Each core has private L1/L2 caches, so two cores could each hold a copy of the same line. A
**coherence protocol** (MESI and variants) keeps them consistent:

| State | Meaning |
| --- | --- |
| **M**odified | only this core has it, and it's dirty |
| **E**xclusive | only this core has it, clean |
| **S**hared | several cores have clean copies |
| **I**nvalid | this copy is stale - must re-fetch |

Before a core can write a line, every other copy must be **invalidated**. If two cores keep
writing to the same line, it ping-pongs between their caches - expensive even if they write
*different* variables that happen to share a line. That's **false sharing**, measured in
[`struct-cacheline-layout`](../../practice/os-concepts/struct-cacheline-layout).

## What this means for code

| Pattern | Why it's fast or slow |
| --- | --- |
| sequential array scan | full use of every line + prefetcher = near bandwidth |
| row-major 2D traversal (C) | sequential in memory |
| column-major 2D traversal (C) | one int used per 64-byte line, new page every access |
| linked list / tree with heap nodes | every hop is a dependent load, often a cache miss |
| hot struct fields spread across lines | more lines fetched per object |
| two threads writing neighbouring counters | false sharing |

## Clarifications

- **Caches are invisible to correctness, visible to performance** (single-threaded). The same
  code gives the same results whether data is in L1 or DRAM - just at very different speeds.
- **Multi-threaded code also needs memory ordering.** Store buffers and out-of-order execution
  mean another core can observe your writes in a different order than you wrote them, unless
  you use atomics or locks (which issue the necessary barriers).
- **Big O hides this.** An O(n) scan over a contiguous array can beat an O(log n) search through
  a pointer-based tree for surprisingly large n.

## Practice project

[`practice/memory-management/cache-locality`](../../practice/memory-management/cache-locality) -
the same 64MB grid summed row-major vs column-major (~20x difference), and a stride sweep
showing the total time stays flat until the stride passes one cache line (16 ints).

## Related

- [The anatomy of memory](the-anatomy-of-memory.md) - what's physically at the bottom of the path.
- [Virtual memory](virtual-memory.md) - step 1 of the path, in depth.
- [Inside the CPU demo: CPU wait times](<../inside the cpu/inside-the-cpu-demo-cpu-wait-times.md>) -
  measuring the cost of each level.
