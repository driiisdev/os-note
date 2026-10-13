# Practice: The Build Pipeline

Concept note: [`compilers-n-linkers.md`](<../../../concepts/os concepts/compilers-n-linkers.md>)

## What this demonstrates

`gcc main.c -o app` hides four separate tools. This project runs each one by hand and keeps
every intermediate file, so you can open them:

1. **Preprocessor** - `main.c` → `main.i`: `#include` pasted in, `#define` substituted.
2. **Compiler** - `main.i` → `main.s`: C becomes assembly for one specific CPU.
3. **Assembler** - `main.s` → `main.o`: assembly becomes machine code, with **holes** for
   anything defined elsewhere.
4. **Linker** - `main.o` + `mathx.o` → `app`: holes filled, one executable.

Then the same tiny library is linked two ways - **static** (`.a`, code copied into the binary)
and **shared** (`.so`, loaded at runtime by the dynamic loader).

## Folder structure

```text
build-pipeline/
  main.c      <- calls square() and cube(), reads the global `calls`
  mathx.h     <- declarations only: what the compiler needs to type-check calls
  mathx.c     <- definitions: what the linker needs to fill the holes
  Makefile    <- `make stages`, `make symbols`, `make static`, `make shared`, `make demo`
```

Build output goes to `build/`.

## Build & run

```bash
make demo          # everything below, in order
make stages        # 1-4, then wc -l on each stage
make symbols       # nm + readelf -r: undefined symbols and relocations
make static        # libmathx.a
make shared        # libmathx.so + ldd + the PLT call
```

## What you'll actually see

```text
$ make stages
   15 main.c
  838 build/main.i         <- 15 lines became 838: that's stdio.h, pasted in
   54 build/main.s
mathx 1.0: square(3) = 9, cube(3) = 27, square() ran 2 times

$ make symbols
== main.o: U = undefined, needs someone else to provide it
                 U calls
                 U cube
0000000000000000 T main
                 U printf
                 U square

== mathx.o: T = code (.text), B = zero-initialized data (.bss), D = initialized data
0000000000000000 B calls
0000000000000011 T cube
0000000000000000 T square

== relocations: holes in main.o the linker must patch with real addresses
000000000000000b  0000000500000004 R_X86_64_PLT32   0000000000000000 square - 4
0000000000000017  0000000600000004 R_X86_64_PLT32   0000000000000000 cube - 4
0000000000000024  0000000700000002 R_X86_64_PC32    0000000000000000 calls - 4
000000000000004a  0000000800000004 R_X86_64_PLT32   0000000000000000 printf - 4

== after linking: the same symbols now have addresses
0000000000004014 B calls
00000000000011b3 T cube
0000000000001149 T main
00000000000011a2 T square

$ make static
-rwxr-xr-x 1 driiisdev driiisdev 16080 Oct 12 15:12 build/app-static
	linux-vdso.so.1 (0x00007fff09d6b000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f88e4c7a000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f88e4e98000)

$ make shared
	linux-vdso.so.1 (0x00007ffdc51a4000)
	libmathx.so => /home/.../build-pipeline/build/libmathx.so (0x00007f2b7fe5f000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f2b7fc48000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f2b7fe6b000)
== the call to square() now goes through the PLT, resolved by ld.so
    119a:	call   1070 <square@plt>
```

- **`U`** = "I use this but don't have it". `main.o` can't run on its own: `square`, `cube`,
  `calls` and `printf` are just names with zeros where the addresses go.
- **Relocations** are the to-do list for the linker: "at offset `0xb` in `.text`, write the
  address of `square`". After linking, every symbol has a real address.
- **`app-static` has no `libmathx` in `ldd`** - its code was copied in at link time. It still
  needs `libc.so.6`, because only *our* library was static. (`gcc -static` would copy libc in
  too and produce a ~800KB binary with no dependencies at all.)
- **`app-shared`** needs `libmathx.so` at runtime. Delete the `.so` and it won't start - the
  **dynamic loader** (`ld-linux-x86-64.so.2`) fails before `main` ever runs.

## Code walkthrough

```c
// main.c
#include "mathx.h"     // declarations: `int square(int x);` - enough to type-check the call
extern int calls;      // "this exists somewhere else" - becomes an undefined symbol in main.o
```

The `shared` target is the interesting one:

```make
$(CC) -O1 -fPIC -shared mathx.c -o build/libmathx.so
$(CC) main.c -Lbuild -lmathx -Wl,-rpath,'$ORIGIN' -o build/app-shared
```

- `-fPIC` - **position-independent code**: the library can be loaded at any address, because
  it accesses globals through a table (the GOT) instead of hard-coded addresses.
- `-L` / `-l` - where to find the library at *link* time.
- `-Wl,-rpath,'$ORIGIN'` - where to find it at *run* time: "the directory the executable is in".
- `call square@plt` - calls go through the **Procedure Linkage Table**, a small stub that
  jumps through a GOT slot `ld.so` fills in with the real address.

## Try this yourself

- `less build/main.i` - scroll to the bottom to find your code after 800+ lines of `stdio.h`.
- Delete the `extern int calls;` line in `main.c`: a **compiler** error. Keep it but delete
  `int calls = 0;` from `mathx.c`: a **linker** error (`undefined reference to 'calls'`). Two
  different stages, two different kinds of error.
- `LD_DEBUG=bindings build/app-shared 2>&1 | grep square` - watch `ld.so` resolve `square`
  at runtime.
- Edit `mathx.c` to print something, rebuild **only** `libmathx.so`, and rerun `app-shared`
  without relinking it - the change shows up. Try the same with `app-static` - it doesn't.

## Real-world equivalent

- Every C/C++/Rust/Go build is this pipeline. `cmake`, `cargo` and `go build` orchestrate it.
- Shared libraries are why a single security fix to `libssl.so` patches every program on the
  system at once - and why "DLL hell" exists. Static linking (Go's default, Rust's default for
  Rust code, Alpine/musl static builds for containers) trades that for self-contained binaries.
- `LD_PRELOAD` (inject a library that overrides functions like `malloc`) only works because
  symbols in shared libraries are resolved at runtime.
