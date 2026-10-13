# Practice: Page Table Walker

Concept notes:
[`page-tables.md`](<../../../concepts/os concepts/page-tables.md>) ·
[`virtual-memory.md`](<../../../concepts/memory management/virtual-memory.md>)

## What this demonstrates

1. **`walker`** - a simulated x86-64 MMU: a **4-level page table** (9 + 9 + 9 + 9 bits of index
   plus a 12-bit offset) with a **16-entry TLB** in front of it. You'll watch an address get
   split into indices, a TLB miss turn into a 4-step page walk, a page fault get handled, a
   write to a read-only page get refused, how much memory the page tables themselves cost for
   dense vs sparse mappings, and how the TLB hit rate collapses with random access.
2. **`pte-bloat`** - the real kernel, measured: 8 processes share one 512MB memory region, and
   every one of them pays for its *own* page tables to map it. This is the
   "process-per-connection + big shared buffer pool" problem Postgres runs into.

## Folder structure

```text
page-table-walker/
  walker.c      <- the simulated MMU: walk(), map_page(), translate(), the TLB
  pte-bloat.c   <- N forked workers touching one MAP_SHARED region; reads VmPTE for each
  Makefile      <- `make`, `make demo`
```

## Build & run

```bash
make demo
./bin/pte-bloat 32 512     # 32 workers, 512MB shared
```

## What you'll actually see

```text
1) splitting a virtual address: 0x00007f1234567abc
   PML4 254 | PDPT 72 | PD 418 | PT 359 | offset 0xabc

2) the same address, accessed three times
  access #1
    TLB miss -> page walk (4 extra memory reads):
      PML4[254] -> missing
    PAGE FAULT: not mapped -> OS allocates a frame, retries
      PML4[254] -> present
      PDPT[ 72] -> present
      PD  [418] -> present
      PT  [359] -> leaf entry
    -> pa 0x5abc (cached in TLB)
  access #2
    TLB hit  -> pa 0x5abc
  access #3
    TLB hit  -> pa 0x5abc
  read-only page, then a write to it:
    TLB miss -> page walk (4 extra memory reads):
      ...
    PROTECTION FAULT: write to read-only page -> SIGSEGV

3) page-table memory: 512 pages (2MB) dense vs spread 1GB apart
   dense : 512 data pages need   4 page-table pages (16 KB of tables)
   sparse: 512 data pages need 1026 page-table pages (4104 KB of tables)

4) TLB hit rate: 100k accesses, sequential vs random over 64MB
   sequential (64B steps):  98.4% TLB hits
   random                :   0.1% TLB hits (99916 page walks)
```

```text
$ ./bin/pte-bloat 8 512
shared region: 512 MB, populated once by the parent

  worker        VmPTE       RssShmem
  1574        1072 KB      522464 KB
  1575        1072 KB      521184 KB
  ...
  1581        1072 KB      522976 KB

  8 workers x ~1072 KB = 8576 KB of page tables, all describing the SAME 512 MB.
  with 2MB huge pages each worker would need ~1/512 of that.
```

- **Same physical frame offset (`0xabc`) as the virtual offset** - translation only replaces
  the page number; the low 12 bits pass straight through.
- **Dense: 4 table pages map 2MB. Sparse: 1,026 table pages map the same 2MB** of data. The
  tree only allocates the branches you use, so address-space layout decides page-table cost.
- **98.4% TLB hits sequentially** = 63 of every 64 accesses land on the page just translated.
  **Random over 64MB: 0.1%** - 16 entries can't cover 16,384 pages, so almost every access
  pays a full page walk.
- **`pte-bloat`**: the 512MB of data exists **once** in RAM, but each worker built ~1MB of page
  tables (512MB / 4KB = 131,072 entries × 8 bytes). With 500 Postgres connections touching an
  8GB `shared_buffers`, that's 500 × 16MB = **8GB of page tables** - as much as the data itself.

## Code walkthrough

```c
static int idx(uint64_t va, int level) { return (va >> (12 + 9 * level)) & 0x1ff; }
//  level 3 = PML4: bits 39-47 | level 2 = PDPT: 30-38 | level 1 = PD: 21-29 | level 0 = PT: 12-20
```

```c
static pte_t *walk(uint64_t va, int create) {
    pte_t *table = frames[cr3];                    // CR3 holds the top-level table's address
    for (int level = 3; level > 0; level--) {
        pte_t *e = &table[idx(va, level)];
        if (!(*e & PRESENT)) {
            if (!create) return NULL;              // CPU: "page fault"
            *e = (alloc_frame(1) << 12) | PRESENT | WRITABLE;   // OS: allocate a table
        }
        table = frames[*e >> 12];                  // follow the pointer one level down
    }
    return &table[idx(va, 0)];                     // the leaf: physical frame + permission bits
}
```

`translate()` is the CPU side: check the TLB, walk on a miss, raise a page fault if the leaf
isn't present (the "OS" maps it and retries), refuse writes to read-only pages, then cache the
result in the TLB with LRU eviction.

## Try this yourself

- In `walker.c`, add **2MB huge pages**: stop the walk at the PD level and use 21 bits of offset.
  Rerun test 4 - the random hit rate should jump, since 16 entries now cover 32MB.
- Add an **accessed bit** and implement a simple page-replacement policy (clock algorithm) when
  "physical memory" is full.
- `./bin/pte-bloat 1 512` vs `./bin/pte-bloat 32 512` - page-table memory scales with the
  number of processes, not the amount of data.
- `grep -E 'PageTables|HugePages' /proc/meminfo` - the system-wide page-table total, live.

## Real-world equivalent

- **Postgres** recommends `huge_pages = on` for large `shared_buffers` largely because of this
  page-table bloat across backends - with 2MB pages each backend needs 512x fewer entries.
- **Linux 5-level paging** (`la57`, a 5th level above PML4) exists for machines with more than
  128TB of address space - each extra level is one more memory read on a TLB miss.
- **KPTI** (the Meltdown fix) gives every process two sets of page tables and switches between
  them on every syscall - which is part of why syscalls got slower in 2018.
- In VMs, every guest page walk goes through **nested page tables** (EPT/NPT), so a TLB miss
  can cost up to 24 memory reads instead of 4. That's a big part of why huge pages matter even
  more for virtualized workloads.
