# Process Section Demo

The lecture demo ties the whole section together by looking at a **real running process**
instead of diagrams. These notes are a reproducible version of that kind of demo - the exact
steps from the lecture can be added here from the `.txt` once they're written down.

## What the demo shows

1. A C program on disk is just an ELF file with sections.
2. Running it creates a process with a PID and an address space.
3. Each variable lands in the region the theory predicts: text, data, bss, heap, stack.
4. The kernel will show you that address space directly through `/proc/<pid>/maps`.
5. A debugger can stop the process mid-execution and show the stack frames and registers.

## Reproducing it

### 1. Look at the program before it runs

```bash
gcc -O0 -g layout.c -o layout
file layout                     # ELF 64-bit LSB pie executable ...
size layout                     # text / data / bss
readelf -S -W layout | grep -E ' \.(text|rodata|data|bss) '
nm layout | grep -E ' [TDBR] '  # symbols: T=text, D=data, B=bss, R=rodata
```

### 2. Run it and look at the process

```bash
./layout &                      # or PAUSE=1 ./layout in the practice project
cat /proc/$!/maps               # the address space, region by region
cat /proc/$!/status | grep -E 'Vm|Threads'   # VmSize (virtual), VmRSS (resident), VmStk...
ls -l /proc/$!/fd               # open file descriptors
pmap -x $!                      # the same map, with sizes and RSS per region
```

Reading `/proc/<pid>/maps`:

```text
558c833d1000-558c833d2000 r-xp 00001000 08:20 12904   /path/to/layout   <- .text
|  start   -   end      | |perm| |offset| |dev| |inode|   |backing file|
```

- **perm**: `r`ead, `w`rite, e`x`ecute, `p`rivate (copy-on-write) or `s`hared.
- **offset/inode**: which file and where in it the region comes from; `0` and no name means
  anonymous memory (heap, bss tail, big mallocs, stacks).
- Named pseudo-regions: `[heap]`, `[stack]`, `[vdso]`, `[vvar]`.

### 3. Stop it mid-execution with a debugger

```bash
gdb -q ./add
(gdb) break add
(gdb) run
(gdb) bt                        # the call stack: add <- main
(gdb) info registers rip rsp rbp
(gdb) info frame                # saved rbp, saved rip (return address), locals
(gdb) x/2gx $rbp                # raw: [rbp] = caller's rbp, [rbp+8] = return address
(gdb) disassemble main          # find the instruction right after `call add`
(gdb) finish                    # run until add returns; prints the return value
```

🖼️ **/proc maps annotated:**
![proc maps annotated](<../../assets/img/proc-maps-annotated.png>)

## What to notice

- The **program image** sits at a low random address (`0x55...`, PIE + ASLR), the
  **libraries and big mallocs** near `0x7f...`, and the **stack** at the top (`0x7ffc...`).
- Permissions follow **W^X**: code is never writable, data is never executable.
- `VmSize` (virtual) is far bigger than `VmRSS` (resident) - most of the address space is
  reserved but untouched.
- Two runs of the same program give different addresses (ASLR) but the same layout.
- Two *processes* of the same program share their `libc.so` code pages in physical memory,
  even though each sees them at a different virtual address.

## Practice projects

- [`practice/anatomy-of-process/process-memory-map`](../../practice/anatomy-of-process/process-memory-map) -
  steps 1 and 2: `make inspect`, `./bin/layout`, `PAUSE=1 ./bin/layout`, `./bin/heap`.
- [`practice/anatomy-of-process/stack-frames`](../../practice/anatomy-of-process/stack-frames) -
  step 3: `make step` runs the gdb session above from a script.

## Related

- [Anatomy of a process](anatomy-of-a-process.md) - the theory this demo confirms.
- [Memory section demo](<../memory management/memory-section-demo.md>) - the next demo, one
  layer down: what's behind those virtual addresses.
