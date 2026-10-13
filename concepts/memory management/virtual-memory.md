# Virtual Memory

Every address a user program ever sees - every pointer, every `&x`, every value in `rip` or
`rsp` - is a **virtual address**. The CPU's **memory management unit (MMU)** translates each
one into a **physical address** in RAM, on every single access, using **page tables** the OS
sets up per process. That one layer of indirection is what gives every process its own private
address space.

## Pages and frames

Memory is managed in fixed-size blocks:

- a **page** - a 4KB block of *virtual* address space (x86-64 and most ARM; 16KB on Apple
  Silicon);
- a **frame** - a 4KB block of *physical* RAM.

The page table maps page → frame (or "not present"), plus permission bits for each page.

```text
 virtual address (x86-64, 48 bits used)
 +---------+---------+---------+---------+--------------+
 | PML4 9b | PDPT 9b |  PD 9b  |  PT 9b  |  offset 12b  |
 +---------+---------+---------+---------+--------------+
      \_________ virtual page number _________/      \__ same in the physical address

 physical address = (frame number from the page table) << 12 | offset
```

The table is actually a **4-level tree** (5 levels on very large machines), so the OS only
needs to build the branches a process actually uses - see
[page tables](<../os concepts/page-tables.md>) for the full walk.

## The TLB: making translation fast

Walking 4 levels of table on every access would mean 4 extra memory reads per access. The
**Translation Lookaside Buffer** caches recent translations inside the core - typically 64
entries in a first-level TLB and 1,000-2,000 in a second level. Most accesses hit; a **TLB
miss** triggers a hardware page walk. Switching to another process changes the page tables, so
TLB entries must be flushed or tagged (PCID on x86, ASID on ARM).

## Page faults

If the walk finds the page isn't present, or the access violates its permissions, the CPU raises
a **page fault** - an exception that runs the kernel's fault handler. Many faults are perfectly
normal; they're how virtual memory does its job lazily:

| Fault cause | Kernel's response | Kind |
| --- | --- | --- |
| first touch of malloc'd/anonymous memory | allocate a zeroed frame, map it | minor |
| first read of untouched anonymous memory | map the shared **zero page** (no allocation) | minor |
| page of a mapped file already in the page cache | map that cached page | minor |
| page of a mapped file **not** in RAM | read it from disk, then map it | **major** |
| page was swapped out | read it back from swap | **major** |
| write to a copy-on-write page (after `fork`) | copy the page, map the copy writable | minor |
| access to an unmapped address, or write to read-only | send **SIGSEGV** - "segmentation fault" | error |

🖼️ **page fault flow:**
![page fault flow](<../../assets/img/page-fault-flow.png>)

## What virtual memory buys you

- **Isolation** - process A has no page-table entry for process B's frames, so it simply can't
  address them. The kernel's pages are mapped in every process but marked supervisor-only.
- **A clean, identical layout for every process** - every program can be linked as if it owns
  the whole address space.
- **No need for contiguous physical memory** - a 1GB array can be scattered across frames
  anywhere in RAM.
- **Lazy allocation (demand paging)** - memory costs nothing until it's touched.
- **Sharing** - one physical copy of `libc.so`'s code mapped into hundreds of processes;
  shared memory between processes (`mmap(MAP_SHARED)`, `shm_open`).
- **Copy-on-write** - `fork()` copies page tables, not memory; pages are duplicated only when
  someone writes.
- **Memory-mapped files** - `mmap` a file and read it like an array; the page cache does the I/O.
- **Swap** - rarely-used pages can be moved to disk to free RAM for active ones.
- **Protection bits** - read/write/execute per page: non-executable stacks, read-only code
  (W^X), guard pages.

## What it costs

- **TLB misses** - random access over a large working set means frequent page walks.
- **Page-table memory** - 8 bytes per 4KB page per process; for big shared regions mapped by
  many processes it adds up (the Postgres case in [page tables](<../os concepts/page-tables.md>)).
- **Page-fault overhead** - each first touch costs a trap into the kernel (~1-3µs).
- **Swapping** - if the working set exceeds RAM, the system **thrashes**: constantly paging in
  and out, with every major fault costing a disk read.

**Huge pages** (2MB or 1GB) reduce all three: one TLB entry covers 512x more memory, one fault
populates 2MB, and page tables shrink. Linux offers them explicitly (`hugetlbfs`) and
automatically (**transparent huge pages**, THP).

## Clarifications

- **Virtual memory ≠ swap.** People often say "virtual memory" to mean "disk used as extra
  RAM". Swap is just one feature built on top; virtual memory is the address translation, and
  it's always on.
- **`VmSize` vs `VmRSS`.** A process's virtual size counts everything mapped; RSS counts only
  what's resident in RAM. A 10GB VmSize with a 200MB RSS is normal.
- **Overcommit.** Linux lets processes reserve more memory than exists, betting most won't be
  touched. If too much is, the **OOM killer** terminates a process to free memory.
- **The kernel uses virtual memory too** - it has its own mappings, including a direct map of
  all physical RAM on x86-64.

## Practice projects

- [`practice/memory-management/virtual-memory-lab`](../../practice/memory-management/virtual-memory-lab) -
  same virtual address, two values after `fork()` (copy-on-write); 65,536 page faults for 256MB
  of 4KB pages vs ~640 with huge pages; reads of untouched memory mapping the zero page.
- [`practice/os-concepts/page-table-walker`](../../practice/os-concepts/page-table-walker) -
  a simulated MMU: the 4-level walk, the TLB, page faults and protection faults.

## Related

- [Page tables](<../os concepts/page-tables.md>) - the data structure in full detail.
- [Reading and writing from and to memory](reading-n-writing-from-n-to-memory.md) - the TLB is
  step 1 of every load.
- [Process vs thread](<../process management/process-vs-thread.md>) - threads share one set of
  page tables, processes each have their own.
- [Context switching](<../process management/context-switching.md>) - switching processes means
  switching page tables.
