# Page Tables

[Virtual memory](<../memory management/virtual-memory.md>) says "the MMU translates every virtual
address using page tables". This note is about **the page tables themselves**: what they look
like, how the CPU walks them, what they cost, and how that cost shows up in real systems.

## Why a tree, not an array

A flat table for a 48-bit address space with 4KB pages would need 2^36 entries × 8 bytes = **512GB
per process**. But processes use only small, scattered parts of their address space (code near
the bottom, stack near the top, libraries in between). So the table is a **sparse tree**: only
branches covering used addresses exist.

## x86-64 four-level paging

```text
 48-bit virtual address
 | 47 ... 39 | 38 ... 30 | 29 ... 21 | 20 ... 12 | 11 ...... 0 |
 |  PML4 (9) |  PDPT (9) |   PD (9)  |   PT (9)  | offset (12) |

 CR3 ──> PML4 table (512 entries)
           [idx] ──> PDPT table (512 entries)      each entry covers 512GB
                       [idx] ──> PD table          each entry covers 1GB   (or a 1GB huge page)
                                   [idx] ──> PT    each entry covers 2MB   (or a 2MB huge page)
                                               [idx] ──> 4KB physical frame
 physical address = frame address + offset
```

- Every table is exactly **one 4KB page** of **512 × 8-byte entries** (9 bits of index).
- **`CR3`** holds the physical address of the top-level table. Switching processes = loading a
  new `CR3`.
- Machines with huge address spaces use **5-level paging** (57-bit addresses, `la57`).

🖼️ **4-level page walk:**
![page walk](<../../assets/img/page-walk.png>)

## What's in a page-table entry

| Bit(s) | Meaning |
| --- | --- |
| **Present (P)** | is this mapped? If 0, any access → page fault |
| **Read/Write (R/W)** | 0 = read-only (writes fault - used for copy-on-write) |
| **User/Supervisor (U/S)** | 0 = kernel-only |
| **Accessed (A)** | set by the CPU on any access - used by the kernel to find cold pages to reclaim |
| **Dirty (D)** | set by the CPU on write - this page must be written back before reuse |
| **Page Size (PS)** | in a PD/PDPT entry: this is a 2MB / 1GB huge page, stop the walk here |
| **NX / XD** (bit 63) | no-execute - data pages can't run as code |
| **bits 12-51** | physical frame number |

## The walk, and the TLB

On a TLB miss the hardware **page walker** reads up to 4 entries - 4 dependent memory accesses -
before the actual load or store can happen. The page-table entries themselves are cached in the
regular data caches (and the CPU has small caches of upper-level entries), so a walk is often
much cheaper than 4 RAM accesses - but random access over a large working set still pays heavily.
In a VM with nested paging, a full walk can take up to **24** memory accesses.

## What page tables cost

**Memory.** One 4KB PT page maps 2MB, so mapping memory with 4KB pages costs about **0.2% of the
mapped size per process**: 8 bytes per 4KB. That's small for one process - but it's paid **per
process, per mapping**:

```text
 shared memory region: 8 GB (e.g. a database buffer pool)
 processes mapping all of it: 500
 page tables: 500 × (8 GB / 4 KB × 8 bytes) = 500 × 16 MB = 8 GB of page tables
```

This is the well-known problem with **process-per-connection databases** like PostgreSQL: every
backend process that touches `shared_buffers` builds its own page tables for it. With **2MB huge
pages**, each backend needs 512x fewer entries (`huge_pages = on` in Postgres).

**Time.** Building page tables happens in page faults; copying them happens in `fork()` (measured
at 0.2ms → 6.9ms as a parent grows to 1GB in the
[process-vs-thread](../../practice/process-management/process-vs-thread) project); tearing them
down happens in `munmap`/`exit`. TLB **shootdowns** - telling other cores to flush a stale entry
via inter-processor interrupts - make `munmap` and permission changes expensive in multi-threaded
programs.

## Huge pages

| Page size | One TLB entry covers | Page-table entries for 1GB |
| --- | --- | --- |
| 4 KB | 4 KB | 262,144 |
| 2 MB | 2 MB | 512 |
| 1 GB | 1 GB | 1 |

- **Transparent Huge Pages (THP)** - the kernel uses 2MB pages automatically for suitable
  anonymous memory (`/sys/kernel/mm/transparent_hugepage/enabled`). Convenient, but compaction and
  khugepaged can cause latency spikes, so some databases recommend turning it off.
- **hugetlbfs / explicit huge pages** - reserved up front (`vm.nr_hugepages`), requested with
  `MAP_HUGETLB` or `SHM_HUGETLB`. Predictable; what Postgres, Oracle and DPDK use.

## Seeing them

```bash
grep VmPTE /proc/<pid>/status              # this process's page-table memory
grep -E 'PageTables|AnonHugePages|HugePages_' /proc/meminfo   # system-wide
grep -A20 'some-region' /proc/<pid>/smaps  # per-region RSS, huge-page usage
```

## Clarifications

- **The kernel's own memory is in every process's page tables** (the upper half of the address
  space), marked supervisor-only - so a syscall doesn't need to switch tables. KPTI (the Meltdown
  fix) changed that on affected CPUs: user mode now gets a stripped-down table.
- **Page tables are per process, not per thread.** All threads share one `CR3`.
- **Huge pages aren't free** - internal fragmentation (a 2MB page for 4KB of data), and splitting
  or compacting them takes work.

## Practice project

[`practice/os-concepts/page-table-walker`](../../practice/os-concepts/page-table-walker) -
`walker` simulates the 4-level walk with a TLB (index breakdown, faults, protection faults; 4 vs
1,026 table pages for dense vs sparse mappings; 98.4% vs 0.1% TLB hits), and `pte-bloat` measures
8 forked workers each spending ~1MB of page tables on the same 512MB shared region.

## Related

- [Virtual memory](<../memory management/virtual-memory.md>) - the concept page tables implement.
- [Virtualization and containerization](virtualization-n-containerization.md) - nested paging.
- [Context switching](<../process management/context-switching.md>) - switching `CR3` and the TLB.
