# Process Execution with the Stack

[Simple process execution](simple-process-execution.md) followed one function. Real programs
call functions constantly, and every call is a small, precise dance between the CPU and the
[stack](the-stack.md): save where to come back to, build a frame, do the work, tear the frame
down, jump back. This note walks through that dance instruction by instruction.

## The program

```c
int add(int a, int b) {
    int c = a + b;
    return c;
}

int main(void) {
    int x = 1, y = 2;
    int z = add(x, y);
    return z;              // exit status 3
}
```

## The compiled code (`gcc -O0 -masm=intel`)

```asm
main:
    push    rbp                     ; (1) prologue: save caller's rbp
    mov     rbp, rsp                ;     main's frame starts here
    sub     rsp, 16                 ;     reserve room for x, y, z
    mov     DWORD PTR -12[rbp], 1   ; x = 1
    mov     DWORD PTR -8[rbp], 2    ; y = 2
    mov     edx, DWORD PTR -8[rbp]
    mov     eax, DWORD PTR -12[rbp]
    mov     esi, edx                ; (2) 2nd argument -> esi
    mov     edi, eax                ;     1st argument -> edi
    call    add                     ; (3) push return address, jump to add
    mov     DWORD PTR -4[rbp], eax  ; (8) z = return value (in eax)
    mov     eax, DWORD PTR -4[rbp]
    leave                           ; (9) epilogue: rsp = rbp; pop rbp
    ret                             ;     back into libc, which calls exit(z)

add:
    push    rbp                     ; (4) prologue: save main's rbp
    mov     rbp, rsp                ;     add's frame starts here
    mov     DWORD PTR -20[rbp], edi ;     spill a
    mov     DWORD PTR -24[rbp], esi ;     spill b
    mov     edx, DWORD PTR -20[rbp]
    mov     eax, DWORD PTR -24[rbp]
    add     eax, edx                ; (5) a + b
    mov     DWORD PTR -4[rbp], eax  ;     c = a + b
    mov     eax, DWORD PTR -4[rbp]  ; (6) return value -> eax
    pop     rbp                     ; (7) restore main's rbp
    ret                             ;     pop return address into rip
```

(`add` doesn't need `sub rsp` because it calls nothing else - x86-64 Linux lets a "leaf"
function use the 128 bytes below `rsp`, the **red zone**, without moving the stack pointer.)

## Step by step

**(1) main's prologue.** `push rbp` saves the caller's base pointer; `mov rbp, rsp` makes the
current top the base of main's frame; `sub rsp, 16` reserves room for the locals.

**(2) Arguments go into registers.** The x86-64 System V calling convention passes the first
six integer arguments in `rdi, rsi, rdx, rcx, r8, r9` (`edi`/`esi` are their 32-bit halves).
Only a seventh argument and beyond would go on the stack.

**(3) `call add`** does two things in one instruction:
```text
push  <address of the instruction after the call>    ; the return address
jmp   add
```

**(4) add's prologue** builds add's frame directly below main's.

```text
 +----------------------------+  <- main's rbp
 | z (-4)  y (-8)  x (-12)    |  main's frame
 +----------------------------+  <- main's rsp after `sub rsp, 16`
 | return address into main   |  pushed by `call`
 | saved rbp (main's)         |  pushed by add's `push rbp`     <- add's rbp
 | c (-4) ... a (-20) b (-24) |  add's locals (in the red zone)
 +----------------------------+
```

**(5) The work** - one `add` instruction on registers.

**(6) The return value goes in `eax`/`rax`** - by convention, that's where the caller looks.

**(7) Epilogue and `ret`.** `pop rbp` restores main's base pointer. `ret` pops the return
address off the stack into `rip` - execution continues in main, right after the `call`.

**(8) main picks up the result** from `eax`.

**(9) main's epilogue.** `leave` (= `mov rsp, rbp; pop rbp`) discards main's frame, and `ret`
returns into the C runtime, which calls `exit()` with the value in `eax`.

🖼️ **call and return:**
![call and return](<../../assets/img/call-and-return.png>)

## The rules that make it work

The **calling convention** (ABI) is a contract between the caller and the callee:

| Rule (x86-64 System V) | Why it matters |
| --- | --- |
| args in `rdi, rsi, rdx, rcx, r8, r9`, then the stack | code compiled separately - even by different compilers - can call each other |
| return value in `rax` (and `rdx` for 128-bit) | the caller knows where to look |
| `rbx, rbp, r12-r15` are **callee-saved** | a function that uses them must restore them before returning |
| other registers are **caller-saved** | the caller saves them first if it needs them after the call |
| `rsp` 16-byte aligned at each `call` | SIMD instructions require aligned stack data |

Windows x64 uses a different convention (`rcx, rdx, r8, r9` + 32 bytes of "shadow space"),
which is one reason you can't just link Linux and Windows object files together.

## Recursion is nothing special

Each recursive call is just another `call` with another frame. `factorial(5)` has five frames
of `factorial` on the stack at its deepest point, each with its own `n`. That's also why deep
recursion eventually overflows: the stack is finite (see [the stack](the-stack.md)).

## Clarifications

- **The return address is just data on the stack.** That's what makes stack buffer overflows
  dangerous: overwrite it, and `ret` jumps wherever the attacker wants.
- **Frame pointers are optional.** With `-fomit-frame-pointer` (default at `-O2` on many
  compilers), `rbp` becomes a normal register and debuggers unwind the stack using DWARF unwind
  tables instead. Several Linux distros (Fedora, Ubuntu 24.04) turned frame pointers back on so
  that profilers can unwind cheaply.
- **Optimized code often skips most of this.** Small functions get **inlined** (no call at
  all), locals live in registers, and tail calls become plain jumps.

## Practice project

[`practice/anatomy-of-process/stack-frames`](../../practice/anatomy-of-process/stack-frames) -
`make asm` produces exactly the listing above from `add.c`, and `make step` runs it in gdb,
stopping in `main` and `add` to print `rip/rsp/rbp`, `info frame`, and the raw saved-rbp and
return-address words with `x/2gx $rbp`.

## Related

- [The stack](the-stack.md) - the region this all happens in.
- [Simple process execution](simple-process-execution.md) - the single-function version.
- [Compilers and linkers](<../os concepts/compilers-n-linkers.md>) - who decides `add`'s address
  so that `call add` can jump to it.
