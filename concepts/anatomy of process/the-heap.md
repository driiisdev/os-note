# The Heap

The heap is where a process keeps memory whose **size isn't known until runtime** or whose
**lifetime doesn't match a function call** - a buffer sized from user input, a node in a linked
list, a response object passed between threads. You ask for it (`malloc`, `new`), you use it
through a pointer, and it stays until you give it back (`free`, `delete`) or a garbage
collector decides you're done.

## Where heap memory comes from

`malloc` is not a system call. It's a library function (glibc's allocator is **ptmalloc**)
that manages memory it got from the kernel in big pieces, using two mechanisms:

| Mechanism | Used for (glibc defaults) | How it works |
| --- | --- | --- |
| **`brk` / `sbrk`** | small allocations | moves the **program break** - the end of the `[heap]` region - upward, growing one contiguous area |
| **`mmap`** | allocations ≥ 128KB (`M_MMAP_THRESHOLD`, adjusts dynamically) | asks the kernel for a fresh, separate region of pages |

```text
            +---------------- [heap] ------------------+
  ... .bss  | chunk | chunk | free | chunk |  (top)    | <- program break (sbrk(0))
            +------------------------------------------+       grows upward ->

            ...                                            (far away, near the libraries)
            +--- mmap'd chunk (e.g. malloc(8MB)) ---+
```

The allocator carves small requests out of the heap, keeps freed chunks in **free lists**
(bins) for reuse, and splits/merges chunks as needed. Each chunk has a small header (its size,
in-use flags) just before the pointer you get back - which is why `free()` doesn't need to be
told the size.

🖼️ **heap internals:**
![heap internals](<../../assets/img/heap-internals.png>)

## Allocated is not the same as resident

`malloc(1 GB)` usually succeeds instantly even on a machine with less free RAM, because the
kernel only reserves **virtual address space**. Physical pages are assigned on **first touch**
(a page fault per page - [demand paging](<../memory management/virtual-memory.md>)). Linux's
default **overcommit** policy allows promising more memory than exists; if programs then touch
too much of it, the **OOM killer** picks a process to kill.

## Why the heap is slower than the stack

- **Bookkeeping**: finding a fitting free chunk, splitting it, updating headers and bins.
- **Thread safety**: many threads share one heap, so allocators need locks or per-thread caches
  (glibc uses multiple arenas and a per-thread cache, *tcache*).
- **Occasional syscalls**: growing the heap (`brk`) or mapping a large chunk (`mmap`) enters the
  kernel; the first touch of each new page takes a page fault.
- **Locality**: consecutive allocations can land far apart, so walking a linked list of heap
  nodes can miss the cache on every node.

Alternative allocators - **jemalloc** (FreeBSD, Redis), **tcmalloc** (Google), **mimalloc**
(Microsoft) - make different trade-offs on speed, fragmentation and multi-threaded scaling.

## Classic heap bugs

| Bug | What happens |
| --- | --- |
| **memory leak** | memory is never freed - RSS grows until the process is restarted or killed |
| **use-after-free** | a pointer is used after `free()` - the chunk may already belong to someone else |
| **double free** | `free()` twice - corrupts the allocator's free lists |
| **heap buffer overflow** | writing past the end of a chunk - corrupts the next chunk's header |
| **fragmentation** | lots of free memory, but in pieces too small to satisfy a request |

Tools: **Valgrind** (`valgrind --leak-check=full`), **AddressSanitizer** (`-fsanitize=address`).
Languages with garbage collection (Java, Go, JS, Python) or ownership rules (Rust) exist largely
to make these bugs impossible or rare - at the cost of GC pauses or a stricter compiler.

## Clarifications

- **"The heap" (memory region) ≠ "a heap" (priority-queue data structure).** Unrelated, same word.
- **`free()` usually doesn't give memory back to the OS.** Small freed chunks stay in the
  allocator for reuse, so RSS often doesn't drop after freeing - which can look like a leak.
  Large mmap'd chunks *are* returned immediately (`munmap`). `malloc_trim()` can release free
  memory at the top of the heap.
- **Every thread shares the heap** - that's what lets a thread allocate an object and hand it to
  another thread - but it's also why heap data needs synchronization.
- **Garbage-collected languages still have a heap** - the GC is just the thing calling "free".

## Practice project

[`practice/anatomy-of-process/process-memory-map`](../../practice/anatomy-of-process/process-memory-map) -
`./bin/heap` shows the program break moving for small mallocs, a 64MB malloc landing in its own
mmap region without moving the break at all, RSS staying flat until the memory is touched, and
50MB leaking in a loop.

## Related

- [The stack](the-stack.md) - the fast alternative when the lifetime matches a call.
- [Data section](data-section.md) - the other fixed alternative, for whole-program lifetimes.
- [Virtual memory](<../memory management/virtual-memory.md>) - how pages behind the heap are
  actually provided.
