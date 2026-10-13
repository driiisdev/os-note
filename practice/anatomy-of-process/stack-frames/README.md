# Practice: Stack Frames

Concept notes:
[`simple-process-execution.md`](<../../../concepts/anatomy of process/simple-process-execution.md>) ·
[`the-stack.md`](<../../../concepts/anatomy of process/the-stack.md>) ·
[`process-execution-with-stack.md`](<../../../concepts/anatomy of process/process-execution-with-stack.md>) ·
[`process-section-demo.md`](<../../../concepts/anatomy of process/process-section-demo.md>)

## What this demonstrates

- How a CPU executes a function call: the **instructions** the compiler emits for locals,
  arguments, `call` and `ret` (`make asm`), and the **registers** that move while it happens
  (`make step`, with gdb).
- That the stack **grows down**, one frame per call, with each frame holding locals plus the
  saved base pointer and **return address**.
- That the stack is **finite** (`ulimit -s`, 8MB by default on Linux) - and what overflowing it
  looks like.
- Why returning a pointer to a local variable is a bug: the frame is gone the moment the
  function returns, and the next call reuses the same bytes.

## Folder structure

```text
stack-frames/
  add.c       <- the smallest program worth stepping through: main() calls add(1, 2)
  step.gdb    <- a gdb script: breaks in main and add, prints rip/rsp/rbp and the raw stack
  stack.c     <- three experiments: frames | overflow | dangling
  Makefile    <- `make`, `make asm`, `make step` (needs gdb)
```

Everything is compiled with `-O0 -fno-omit-frame-pointer` - with optimizations on, `add(1, 2)`
becomes the constant `3` and there's no stack activity left to look at.

## Build & run

```bash
make
./bin/add; echo "exit status: $?"     # 3 - the return value of main
make asm                               # the compiler's output, Intel syntax
./bin/stack frames
./bin/stack overflow
./bin/stack dangling
sudo apt install gdb && make step
```

## What you'll actually see

`make asm` (trimmed to the interesting part):

```asm
add:
    push    rbp                     ; save the caller's frame base
    mov     rbp, rsp                ; my frame starts here
    mov     DWORD PTR -20[rbp], edi ; spill argument a (arrived in edi)
    mov     DWORD PTR -24[rbp], esi ; spill argument b (arrived in esi)
    mov     edx, DWORD PTR -20[rbp]
    mov     eax, DWORD PTR -24[rbp]
    add     eax, edx                ; the actual work: one instruction
    mov     DWORD PTR -4[rbp], eax  ; c = a + b
    mov     eax, DWORD PTR -4[rbp]  ; return value goes in eax
    pop     rbp                     ; restore the caller's frame base
    ret                             ; pop the return address into rip
main:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 16                 ; reserve room for x, y, z
    mov     DWORD PTR -12[rbp], 1   ; x = 1
    mov     DWORD PTR -8[rbp], 2    ; y = 2
    mov     edx, DWORD PTR -8[rbp]
    mov     eax, DWORD PTR -12[rbp]
    mov     esi, edx                ; 2nd argument register
    mov     edi, eax                ; 1st argument register
    call    add                     ; push return address, jump
    mov     DWORD PTR -4[rbp], eax  ; z = result
    mov     eax, DWORD PTR -4[rbp]
    leave                           ; mov rsp, rbp ; pop rbp
    ret
```

```text
$ ./bin/stack frames
nested calls - watch the addresses go DOWN:
  depth 1: &local=0x7ffcc938bfd4  frame=0x7ffcc938bff0  return-to=0x562fc6a1b5c7  (72 bytes below main)
  depth 2: &local=0x7ffcc938bf94  frame=0x7ffcc938bfb0  return-to=0x562fc6a1b332  (136 bytes below main)
  depth 3: &local=0x7ffcc938bf54  frame=0x7ffcc938bf70  return-to=0x562fc6a1b332  (200 bytes below main)
  depth 4: &local=0x7ffcc938bf14  frame=0x7ffcc938bf30  return-to=0x562fc6a1b332  (264 bytes below main)
  depth 5: &local=0x7ffcc938bed4  frame=0x7ffcc938bef0  return-to=0x562fc6a1b332  (328 bytes below main)

$ ./bin/stack overflow
stack limit (ulimit -s): 8192 KB
  SIGSEGV at depth 8059 - 8184 KB of stack used, ~1040 bytes per frame

$ ./bin/stack dangling
right after return : *p = 1234   (looks fine - the bytes weren't reused yet)
after another call : *p = -1   (the next call's frame overwrote it)
```

Every frame is exactly 64 bytes lower than its caller. Depth 1's return address points into
`main`; depths 2-5 all return to the *same* instruction inside `frame()` - the line right
after the recursive call.

## Code walkthrough

**Watching the frames** - two gcc builtins read the frame registers directly:

```c
void *fp  = __builtin_frame_address(0);   // rbp: the base of THIS frame
void *ret = __builtin_return_address(0);  // [rbp+8]: where `ret` will jump back to
```

**Surviving a stack overflow long enough to report it** - when the stack is full, the SIGSEGV
handler has nowhere to run. So the program gives it a separate stack first:

```c
static char altstack[64 * 1024];
stack_t ss = { .ss_sp = altstack, .ss_size = sizeof altstack };
sigaltstack(&ss, NULL);                                       // "run handlers over here"
struct sigaction sa = { .sa_handler = on_segv, .sa_flags = SA_ONSTACK };
sigaction(SIGSEGV, &sa, NULL);
```

The overflow is caught because the kernel leaves a **guard gap** of unmapped pages below the
stack - touching it is an ordinary page fault the kernel refuses to satisfy.

**The dangling pointer** - `bad_pointer()` returns `&value`; `overwrite_stack()` then runs with
its frame at the *same address* and fills it with `-1`. Nothing "freed" the memory - the stack
pointer simply moved back up, and the next call reused the space.

## Try this yourself

- `ulimit -s 1024; ./bin/stack overflow` - a 1MB stack overflows at ~1000 frames. `ulimit -s`
  only affects that shell session.
- Change `volatile char buf[1000]` to `buf[100]` and watch the depth go up ~10x.
- In gdb (`make step`), after hitting `add`, run `x/2gx $rbp`: the first value is main's saved
  `rbp`, the second is the return address - compare it with `disassemble main` and find the
  instruction right after `call add`.
- Rebuild `add.c` with `-O2` (`gcc -O2 -S -masm=intel add.c -o -`) - `main` becomes
  `mov eax, 3; ret`. No frame, no call, no stack.

## Real-world equivalent

Every stack trace you've ever read - a Java exception, a Python traceback, a core dump in gdb -
is produced by walking exactly these frames: follow saved `rbp` (or DWARF unwind info when frame
pointers are omitted) from frame to frame, and map each return address back to a function name.
Profilers like `perf` do the same thing thousands of times per second, which is why many Linux
distros now build with `-fno-omit-frame-pointer`. Stack size limits are why threads in Go start
with tiny 8KB growable stacks, and why deep recursion crashes C and Java but not Go.
