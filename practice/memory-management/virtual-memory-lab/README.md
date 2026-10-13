# Practice: Virtual Memory Lab

Concept notes:
[`virtual-memory.md`](<../../../concepts/memory management/virtual-memory.md>) ·
[`memory-section-demo.md`](<../../../concepts/memory management/memory-section-demo.md>) ·
[`page-tables.md`](<../../../concepts/os concepts/page-tables.md>)

## What this demonstrates

1. **A virtual address only means something inside one process.** After `fork()`, parent and
   child print the *same* address for a variable but see *different* values - because each has
   its own page tables, and the first write triggers **copy-on-write**.
2. **Demand paging.** `mmap(256MB)` costs nothing; RAM gets assigned one **page fault** at a
   time as pages are first touched. You'll count the faults, watch RSS grow, and watch the
   kernel build **page tables** (`VmPTE`) along the way.
3. **Huge pages.** The same 256MB with 2MB transparent huge pages takes ~100x fewer faults.
4. **The zero page.** Only *reading* untouched memory faults too - but the kernel maps one
   shared page of zeros, so RSS doesn't grow at all.

## Folder structure

```text
virtual-memory-lab/
  cow.c       <- fork + copy-on-write: same address, two values
  demand.c    <- mmap 256MB, touch every page, count faults / RSS / page-table size
  Makefile    <- `make`, `make demo` (runs every experiment)
```

## Build & run

```bash
make demo
# or individually:
./bin/cow
./bin/demand            # 4KB pages, writes
./bin/demand huge       # 2MB transparent huge pages
./bin/demand 4k read    # reads only
```

## What you'll actually see

```text
$ ./bin/cow
parent (pid 405): &value = 0x55d6e8f6e010, value = 100
child  (pid 407): &value = 0x55d6e8f6e010, value = 200   <- same address, new value
                 the write cost 1 page fault(s): that's copy-on-write
parent (pid 405): &value = 0x55d6e8f6e010, value = 100   <- untouched
```

```text
$ ./bin/demand
  start                  RSS    1080 KB   page tables (VmPTE)    44 KB   minor faults      53
  after mmap(256MB)      RSS    1080 KB   page tables (VmPTE)    44 KB   minor faults      61
  after writing each pg  RSS  263664 KB   page tables (VmPTE)   556 KB   minor faults   65600

  4KB pages, write: 65539 faults for 65536 pages touched, 180.7 ms total (~2.76 us per fault)
  after munmap           RSS    1652 KB   page tables (VmPTE)    44 KB   minor faults   65602

$ ./bin/demand huge
  after writing each pg  RSS  263820 KB   page tables (VmPTE)   556 KB   minor faults     704
  huge pages, write: 642 faults for 65536 pages touched, 27.4 ms total (~42.69 us per fault)

$ ./bin/demand 4k read
  after reading each pg  RSS    1680 KB   page tables (VmPTE)   564 KB   minor faults   65601
  4KB pages, read: 65539 faults for 65536 pages touched, 30.2 ms total (~0.46 us per fault)
```

What to notice:

- **`mmap` itself changed nothing** - not RSS, not page tables. It only reserved a range of
  virtual addresses.
- **65,536 pages → ~65,536 faults.** Each fault takes ~3us: trap into the kernel, find a free
  physical frame, zero it, write the page-table entry, return.
- **Huge pages: ~640 faults instead of 65,536**, and the whole thing is 6-7x faster. Each fault
  is slower (zeroing 2MB instead of 4KB) but there are 512x fewer of them. The remaining
  ~500 faults are the unaligned edges of the region that couldn't be covered by a 2MB page.
- **`VmPTE` is still 556KB with huge pages** - that's a Linux detail: for every huge page the
  kernel pre-deposits a page-table page so it can later split the huge page back into 4KB pages
  without needing to allocate memory at that moment.
- **Reads fault but don't grow RSS** - every read maps the same shared zero page. The page
  tables are still built (564KB), which is a hint that page tables themselves cost memory, the
  subject of [page tables](<../../../concepts/os concepts/page-tables.md>).

## Code walkthrough

```c
pid_t pid = fork();
if (pid == 0) {
    long before = minor_faults();
    value = 200;              // the page holding `value` is shared and marked READ-ONLY
    long after = minor_faults();  // -> write faults -> kernel copies the page -> retries
```

`fork()` doesn't copy memory; it copies **page tables** and marks every writable page
read-only in both processes. The first write by either side faults, and only then does the
kernel copy that one 4KB page. That's why `fork()` + `exec()` is cheap even for big processes.

```c
volatile char *p = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
madvise((void *)p, SIZE, huge ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
for (size_t off = 0; off < SIZE; off += 4096) p[off] = 1;   // one touch per 4KB page
```

The counters come from `getrusage()` (`ru_minflt`) and `/proc/self/status` (`VmRSS`,
`VmPTE`). A **minor** fault is resolved without disk I/O; a **major** fault had to read from
disk (swap, or a file-backed page not in the page cache).

## Try this yourself

- `cat /sys/kernel/mm/transparent_hugepage/enabled` - `[always]` means the kernel will use huge
  pages even without `madvise`. Try `./bin/demand` with the `MADV_NOHUGEPAGE` line removed.
- In `cow.c`, make the child write to a variable on 3 different pages and count the faults.
- Run `./bin/demand` with a `getchar()` added before `munmap` and look at `/proc/<pid>/smaps` -
  find the 256MB region and its `AnonHugePages:` line.

## Real-world equivalent

- **Redis** snapshots its whole dataset with `fork()`: the child writes the snapshot while the
  parent keeps serving - copy-on-write means only pages modified during the snapshot get
  duplicated. This is exactly why Redis docs warn about memory spikes during `BGSAVE`.
- **Databases and JVMs** recommend huge pages (Postgres `huge_pages`, JVM `-XX:+UseLargePages`)
  for big heaps/buffer pools - fewer page faults, smaller page tables, fewer TLB misses.
- Linux **overcommit**: `malloc` of more memory than you have can succeed, because nothing is
  backed until it's touched - and the OOM killer steps in if too much gets touched later.
