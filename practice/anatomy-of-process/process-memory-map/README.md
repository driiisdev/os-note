# Practice: Process Memory Map

Concept notes:
[`program-vs-process.md`](<../../../concepts/anatomy of process/program-vs-process.md>) ·
[`data-section.md`](<../../../concepts/anatomy of process/data-section.md>) ·
[`the-heap.md`](<../../../concepts/anatomy of process/the-heap.md>) ·
[`process-section-demo.md`](<../../../concepts/anatomy of process/process-section-demo.md>)

## What this demonstrates

- The **program** on disk (an ELF file with sections) vs the **process** in memory (an address
  space with regions).
- Where each kind of variable actually lives: **text, rodata, data, bss, heap, mmap, stack** -
  printed as real addresses and matched against the kernel's own map in `/proc/self/maps`.
- How the **heap** grows (the program break moving up), why big allocations skip it entirely,
  and the difference between memory that's *allocated* and memory that's *resident in RAM*.

## Folder structure

```text
process-memory-map/
  layout.c    <- prints every segment's address, lowest first, then /proc/self/maps
  heap.c      <- brk vs mmap, lazy allocation (RSS), and a deliberate leak
  Makefile    <- `make`, `make inspect` (section sizes of the program on disk)
```

## Build & run

```bash
make
./bin/layout
PAUSE=1 ./bin/layout     # stays alive so you can `cat /proc/<pid>/maps` from another terminal
./bin/heap
make inspect             # the PROGRAM: section sizes before it is ever a process
```

## What you'll actually see

```text
$ ./bin/layout
pid 449 - addresses, lowest first:

  text    main()               0x558c833d1321
  rodata  read_only_const      0x558c833d2008
  rodata  "hello" literal      0x558c833d200c
  data    initialized_global   0x558c833d4010
  data    static_local         0x558c833d4014
  bss     zeroed_global        0x558c833d4040
  bss     zeroed_array         0x558c833d4060
  heap    malloc(64)           0x558c84b682a0
  mmap    malloc(8MB)          0x7f44afb24010
  stack   stack_local          0x7ffc3409a3e4
  stack   argv[0] string       0x7ffc3409c840

/proc/self/maps (the kernel's view of the same address space):

558c833d0000-558c833d1000 r--p 00000000 08:20 12904   .../bin/layout   <- ELF header
558c833d1000-558c833d2000 r-xp 00001000 08:20 12904   .../bin/layout   <- .text (executable)
558c833d2000-558c833d3000 r--p 00002000 08:20 12904   .../bin/layout   <- .rodata
558c833d4000-558c833d5000 rw-p 00003000 08:20 12904   .../bin/layout   <- .data
558c833d5000-558c833d9000 rw-p 00000000 00:00 0                        <- rest of .bss (anonymous)
558c84b68000-558c84b89000 rw-p 00000000 00:00 0       [heap]
7f44afb24000-7f44b0328000 rw-p 00000000 00:00 0                        <- the 8MB malloc
7f44b0350000-7f44b04d8000 r-xp 00028000 08:20 46708   .../libc.so.6
7ffc3407c000-7ffc3409d000 rw-p 00000000 00:00 0       [stack]
7ffc341c6000-7ffc341c8000 r-xp 00000000 00:00 0       [vdso]
```

Read the permissions column: code is `r-x` (never writable), constants are `r--`, data is
`rw-` (never executable). That's the OS enforcing W^X with page-table permission bits.

```text
$ ./bin/heap
1) small allocations come from the brk heap - the program break moves up
   program break before: 0x55bf8e843000
   program break after : 0x55bf8e8a6000  (+396 KB for 256 x 2KB mallocs)

2) a big allocation skips the heap and gets its own mmap region
   malloc(64MB) returned 0x7fe08087a010 - program break moved by 0 bytes

3) malloc only reserves address space - RAM is committed on first touch
   RSS after malloc(64MB) :   1004 KB
   RSS after touching 32MB:  35064 KB
   RSS after touching 64MB:  66980 KB
   RSS after free()       :   1664 KB

4) a leak: memory you can no longer free, still counted against the process
   RSS after leaking 50 x 1MB: 52876 KB (run under valgrind to see it reported)
```

```text
$ make inspect
   text	   data	    bss	    dec	    hex	filename
   3952	    712	  16448	  21112	   5278	bin/layout
  [26] .bss              NOBITS          0000000000004020 003020 004040 00  WA  0   0 32
```

`.bss` is 16KB in memory but type **NOBITS** - it takes zero bytes in the file. The loader just
maps zero-filled pages for it.

## Code walkthrough

```c
int initialized_global = 42;          // .data  - value stored in the ELF file
int zeroed_global;                    // .bss   - only its SIZE is stored in the file
static int zeroed_array[4096];        // .bss   - 16KB of zeros, costs nothing on disk
const int read_only_const = 7;        // .rodata - mapped read-only; writing segfaults

int main(int argc, char **argv) {
    static int static_local = 1;      // .data, not the stack - lives for the whole program
    int stack_local = 1;              // main's stack frame
    int *heap_small = malloc(64);     // brk heap
    char *heap_big = malloc(8 << 20); // its own mmap() region (> 128KB threshold)
```

- **Addresses near `0x55...`** are the program image (PIE executables are loaded at a random
  base - ASLR). **Addresses near `0x7f...`** are mmap regions: shared libraries and big
  mallocs. **`0x7ffc...`** is the stack, at the top of user space.
- `heap.c` reads `/proc/self/statm` for the **resident set size (RSS)** - the pages actually in
  RAM. `malloc(64MB)` barely moves it; `memset` does, one page fault at a time.
- `free()` of the 64MB block drops RSS straight back because glibc returns mmap'd chunks to
  the OS with `munmap()`. Small chunks freed back to the brk heap usually *don't* shrink RSS -
  the allocator keeps them for reuse.

## Try this yourself

- Run `./bin/layout` twice. Every address changes (ASLR), but the *distances* between `main`
  and `initialized_global` stay the same - they're fixed offsets inside one ELF file.
- `setarch -R ./bin/layout` disables ASLR for one run - now the addresses repeat.
- Run `PAUSE=1 ./bin/layout` in two terminals and compare both `/proc/<pid>/maps`: same
  program, two processes, two address spaces. `libc.so.6`'s code pages are shared in physical
  RAM between them even though each maps them at its own address.
- `sudo apt install valgrind && valgrind --leak-check=full ./bin/heap` - watch it report
  the 50 lost 1MB blocks with the exact line that allocated them.
- Change `malloc(8 << 20)` to `malloc(100 << 10)` (100KB) and see it land in `[heap]` instead.

## Real-world equivalent

`/proc/<pid>/maps` (and `pmap <pid>`) is exactly what you'd use to debug a real process using
too much memory: is it the heap, an mmap'd file, thread stacks, or a shared library? The
brk-vs-mmap split is why long-running servers sometimes look like they "leak" (RSS stays high
after freeing many small objects) - and why allocators like jemalloc/tcmalloc exist.
