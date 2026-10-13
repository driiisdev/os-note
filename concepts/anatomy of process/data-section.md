# The Data Section

Variables that exist for the **entire life of the program** - globals and `static` variables -
can't live on the [stack](the-stack.md) (frames come and go) and don't need the
[heap](the-heap.md) (their size is known when you compile). They get fixed slots in the
**data segment**, laid out by the compiler and linker and mapped into memory by the loader
before `main` ever runs.

## The sections

```c
int counter = 42;              // .data   - initialized, value stored in the executable file
int total;                     // .bss    - zero-initialized, only the SIZE is stored
static char buffer[1 << 20];   // .bss    - 1MB of zeros: costs ~0 bytes on disk
const double PI = 3.14159;     // .rodata - read-only
const char *name = "osnote";   // `name` (the pointer) in .data, "osnote" (the bytes) in .rodata

void tick(void) {
    static int calls = 0;      // .bss - local SCOPE, but lives for the whole program
    calls++;
}
```

| Section | Contents | In the file? | Permissions in memory |
| --- | --- | --- | --- |
| `.data` | initialized globals/statics | yes - the actual values | read + write |
| `.bss` | zero-initialized globals/statics | **no** - just the total size | read + write |
| `.rodata` | constants, string literals, jump tables | yes | read only |
| `.text` | machine code (not data, but laid out next to it) | yes | read + execute |

**Why have `.bss` at all?** A program with `static char buffer[1 << 20];` would otherwise ship
a megabyte of zeros. Instead, the file just says "I need 1MB of zeroed memory here", and the
kernel maps anonymous zero-filled pages for it. ("bss" originally stood for *Block Started by
Symbol*, an old assembler directive.)

```bash
size ./program          #   text    data     bss     dec    hex filename
                        #   3952     712   16448   21112   5278 ./program
```

🖼️ **data sections:**
![data sections](<../../assets/img/data-sections.png>)

## Properties

- **Fixed size and fixed address** (relative to where the program is loaded). The compiler
  emits instructions that reach a global at a constant offset from the instruction itself
  (`rip`-relative addressing) - fast, with no pointer chasing.
- **Lifetime = the whole program.** Initialized before `main`, destroyed after it returns.
- **Shared by every thread in the process.** That's the dangerous part: two threads doing
  `counter++` on a global is a textbook race condition (see
  [concurrency](<../process management/concurrency.md>)).
- **Not shared between processes.** After `fork()`, the child has its own copy of `.data`
  (copy-on-write - see [virtual memory](<../memory management/virtual-memory.md>)).
- **`const` data really is protected.** `.rodata` pages are mapped read-only, so
  `*(char *)"hello" = 'j';` crashes with a segmentation fault instead of changing the literal.

## Clarifications

- **`static` means two different things in C.** On a global or function, it means *internal
  linkage* (not visible to other `.c` files). On a local variable, it means *static storage
  duration* (lives in `.data`/`.bss`, keeps its value between calls). Both kinds end up in the
  data segment.
- **Initialized to zero → `.bss`, even if you write `= 0`.** `int x = 0;` goes in `.bss` just
  like `int x;`. Only a non-zero initial value forces `.data`.
- **Globals in shared libraries** get the same treatment, but each library has its own data
  segment mapped per process, and they're reached through the GOT (global offset table)
  because the library's load address isn't known until runtime.
- **"Global variables are bad"** is mostly about design (hidden coupling) and concurrency (shared
  mutable state), not about the data section being slow - it's actually one of the fastest
  places to read from.

## Practice project

[`practice/anatomy-of-process/process-memory-map`](../../practice/anatomy-of-process/process-memory-map) -
`./bin/layout` prints the addresses of `.data`, `.bss` and `.rodata` variables next to stack
and heap ones, and `make inspect` shows that the 16KB `.bss` array is type `NOBITS` in the ELF
file - present in memory, absent on disk.

## Related

- [Program vs process](program-vs-process.md) - sections in the file vs regions in memory.
- [The heap](the-heap.md) - for data whose size or lifetime isn't known at compile time.
- [Compilers and linkers](<../os concepts/compilers-n-linkers.md>) - `nm` shows these variables
  as `D` (data), `B` (bss) and `R` (read-only) symbols.
