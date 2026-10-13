# Compilers and Linkers

`gcc main.c -o app` looks like one step, but it's a pipeline of four tools - and a fifth, the
loader, finishes the job every time the program starts. Knowing the stages explains what's in an
executable, the difference between a compiler error and a linker error, and why shared libraries
work the way they do.

## The pipeline

```text
 main.c ──[preprocessor]──> main.i ──[compiler]──> main.s ──[assembler]──> main.o ─┐
 mathx.c ─────────────────────────── ... ──────────────────────────────> mathx.o ─┤
                                                         libc.so / libm.a ... ─────┤
                                                                                   v
                                                                             [linker]
                                                                                   |
                                                                                 app (ELF)
                                                                                   |
                                                       run: [kernel + dynamic loader]
```

### 1. Preprocessor (`cpp`, `gcc -E`)

Pure text substitution: pastes in `#include` files, expands `#define` macros, evaluates `#if`.
A 15-line `main.c` that includes `stdio.h` becomes ~800 lines.

### 2. Compiler (`cc1`, `gcc -S`)

Translates C into **assembly for one specific CPU architecture**:

- **front end** - parse, type-check, build an intermediate representation (IR);
- **optimizer** - inlining, constant folding, dead-code elimination, loop transformations,
  vectorization, register allocation (`-O0` to `-O3`);
- **back end** - emit instructions for the target ISA (x86-64, ARM64, RISC-V...).

### 3. Assembler (`as`, `gcc -c`)

Turns assembly into machine code in a **relocatable object file** (`.o`, ELF type `REL`). It
contains:

- sections: `.text`, `.data`, `.bss`, `.rodata`;
- a **symbol table** - what this file *defines* (`T main`) and what it *uses but doesn't define*
  (`U printf`, `U square`);
- **relocations** - a to-do list of "holes": "at offset 0x0b, insert the address of `square`".

### 4. Linker (`ld`, invoked by `gcc`)

- **Symbol resolution** - match every undefined symbol to exactly one definition, across all
  object files and libraries. None found → `undefined reference to 'square'`. Two found →
  `multiple definition of ...`.
- **Layout** - merge all `.text` sections together, all `.data` together, etc., and assign final
  addresses.
- **Relocation** - fill in every hole with the real address.
- Output: an **executable** (ELF type `EXEC` or `DYN` for position-independent executables) or a
  **shared library**.

🖼️ **Image to add - compile and link pipeline:** the pipeline above with a file icon at each
stage (`.c`, `.i`, `.s`, `.o`, executable), a zoom into `main.o` showing its symbol table with
`U` entries and its relocation list, and arrows from the linker filling those holes with
addresses from `mathx.o`.
![build pipeline](<../../assets/img/build-pipeline.png>)

## Static vs dynamic linking

| | Static (`.a` on Unix, `.lib` on Windows) | Dynamic / shared (`.so`, `.dll`, `.dylib`) |
| --- | --- | --- |
| When library code is added | at **link time** - copied into the executable | at **run time** - loaded by the dynamic loader |
| Executable size | bigger | smaller |
| Dependencies at runtime | none for that library | the `.so` must be present and compatible |
| Security / bug fixes | rebuild every program | replace one `.so`, every program benefits |
| Memory | each process has its own copy | one copy of the code shared by all processes |
| Startup | slightly faster | symbol resolution work at load time |

## Loading and dynamic linking

When you run a dynamically linked program:

1. The kernel (`execve`) maps the executable and reads its `PT_INTERP` header, which names the
   **dynamic loader** (`/lib64/ld-linux-x86-64.so.2`).
2. The kernel starts the *loader* first. It maps every needed `.so` (`ldd app` lists them), then
   resolves symbols between them.
3. Calls into shared libraries go through the **PLT** (procedure linkage table): a small stub
   that jumps via the **GOT** (global offset table), whose slots the loader fills in - by default
   lazily, on the first call.
4. The loader jumps to the program's entry point (`_start` → `__libc_start_main` → `main`).

Shared libraries are compiled as **position-independent code** (`-fPIC`), so they can be mapped
at any address - which is also what makes ASLR possible.

## Other ways to run code

- **Interpreters** (CPython, Ruby MRI) - a program (itself compiled) that reads and executes your
  code or its bytecode at runtime.
- **JIT compilers** (JVM HotSpot, V8, .NET, PyPy) - start interpreting, then compile hot code to
  machine code at runtime, using profiling information an ahead-of-time compiler doesn't have.
- **Cross-compilers** - produce code for a different ISA/OS than the one they run on (building
  ARM firmware on an x86 laptop).

## Clarifications

- **Compiler errors vs linker errors.** "Implicit declaration" or a type mismatch is the compiler;
  "undefined reference" is the linker - the code compiled fine, but a definition is missing.
- **A header file is not a library.** `#include <math.h>` gives the compiler declarations; you
  may still need `-lm` so the linker can find the code.
- **"Statically linked" usually means only some libraries.** Fully static binaries (`gcc -static`,
  Go's default) include libc too and have no runtime dependencies at all.

## Practice project

[`practice/os-concepts/build-pipeline`](../../practice/os-concepts/build-pipeline) - every stage
run by hand with the intermediate files kept (`main.i` is 838 lines); `nm` and `readelf -r`
showing undefined symbols and relocations; the same library linked statically (`.a`) and
dynamically (`.so` + `ldd` + a `call square@plt`).

## Related

- [Program vs process](<../anatomy of process/program-vs-process.md>) - the ELF file the linker
  produces, and the process the loader turns it into.
- [Data section](<../anatomy of process/data-section.md>) - the `.data`/`.bss`/`.rodata` sections
  the linker lays out.
- [Kernel vs user mode switching](kernel-vs-user-mode-switching.md) - `execve` is where the kernel
  takes over.
