# Practice: Syscall Cost

Concept note: [`kernel-vs-user-mode-switching.md`](<../../../concepts/os concepts/kernel-vs-user-mode-switching.md>)

## What this demonstrates

What it costs to cross from **user mode** into **kernel mode** and back, measured over 5 million
calls each:

| Operation | Crosses into the kernel? |
| --- | --- |
| plain function call | no - a `call` and a `ret` |
| `getpid()` via `syscall()` | yes - the cheapest real syscall there is |
| `clock_gettime()` | **no** - looks like a syscall, but runs in user mode via the **vDSO** |
| `write()` 1 byte to `/dev/null` | yes - and goes through the VFS layer to a (trivial) driver |

## Folder structure

```text
syscall-cost/
  syscost.c   <- the four timed loops
  Makefile    <- `make`, `make demo`, `make strace-ls` (needs strace)
```

## Build & run

```bash
make demo
sudo apt install strace && make strace-ls    # every syscall `ls -la /` makes, counted
```

## What you'll actually see

```text
  plain function call               1.4 ns
  getpid() syscall                 60.1 ns   (44x a function call)
  clock_gettime()                  17.9 ns   (vDSO - no mode switch)
  write(/dev/null, 1 byte)         90.9 ns
```

- **~60ns for the cheapest possible syscall** - the kernel does almost nothing for `getpid`,
  so this is nearly all **mode-switch overhead**: the `syscall` instruction, saving user
  registers, security mitigations on entry and exit (on CPUs affected by Meltdown, page-table
  isolation switches page tables on *every* syscall), and `sysret`.
- **`clock_gettime` is 3x cheaper than `getpid`** without ever entering the kernel. The kernel
  maps a small shared library (the vDSO - you saw `[vdso]` in
  [process-memory-map](../../anatomy-of-process/process-memory-map)) into every process, plus
  a read-only page where it keeps the current time updated.
- **`write` to `/dev/null` costs more than `getpid`** even though the data is thrown away:
  file-descriptor lookup, the VFS layer, and a call into the `/dev/null` driver's write
  function.
- WSL2 runs on a hypervisor, which adds a little to everything here; bare-metal numbers are
  usually somewhat lower, with the same ordering.

## Code walkthrough

```c
for (long i = 0; i < N; i++) sink += syscall(SYS_getpid);
```

Using the raw `syscall()` wrapper matters: older glibc versions **cached** `getpid()` in user
space, which would have measured nothing. `syscall(SYS_getpid)` always traps.

```c
__attribute__((noinline)) static long plain_function(long x) {
    __asm__ volatile("" ::: "memory");  // an empty asm "barrier": the compiler must assume it
    return x + 1;                       // has side effects, so it can't delete the loop
}
```

Every result is added to `sink` and printed, so the optimizer can't decide the loops are
pointless and remove them - the classic benchmarking trap.

## Try this yourself

- `strace -c ls -la /` - a whole program's syscall profile. Try `strace -c python3 -c pass`
  and compare.
- `strace -e trace=clock_gettime ./bin/syscost` prints **nothing** - proof that the vDSO calls
  never reach the kernel.
- Add a `read()` of 1 byte from `/dev/zero`, and an `open()`+`close()` of `/dev/null`. Which
  is most expensive, and why? (Hint: path lookup.)
- Run `syscall(SYS_getpid)` in a loop under `perf stat -e cycles:u,cycles:k` (bare-metal Linux
  with perf) to split the time between user and kernel mode.

## Real-world equivalent

- This cost is why **buffering** exists everywhere: `printf` batches output into one `write()`,
  databases batch log records, `sendmmsg`/`recvmmsg` move many packets per syscall.
- It's why **io_uring** was invented: submit many I/O requests through shared memory with one
  (or zero) syscalls instead of one syscall each.
- It's why high-performance networking (DPDK) and storage (SPDK) bypass the kernel entirely
  and poll devices from user space.
- The vDSO trick is why timestamps and `gettimeofday` are cheap enough to call on every
  request in a logging-heavy service.
