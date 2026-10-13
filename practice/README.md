# Practice Projects

Small, runnable C programs (plus two in Python) that make each OS concept **visible** - real
syscalls, real page faults, real context switches, real kernel queues - instead of just
described. Every project folder has its own `README.md` with: what it demonstrates, folder
structure, how to run it, **real output** captured on the machine these notes were written on,
a code walkthrough, things to try, and the real-world system that does the same thing.

## Requirements

- **Linux.** On Windows, use **WSL2** (Ubuntu) - everything here was built and run on
  WSL2 Ubuntu 24.04 with gcc 13.
- `gcc` and `make` - `sudo apt install build-essential`
- `python3` (standard library only) - for `storage-management/toy-filesystem` and
  `os-concepts/kexec-reboot-sim`
- Optional tools some projects point to: `gdb`, `strace`, `valgrind`, `iptables`, `dwarves`
  (`pahole`) - `sudo apt install gdb strace valgrind iptables dwarves`

**Work from the Linux filesystem, not `/mnt/c`.** Copy the folder into your WSL home directory
first (`cp -r /mnt/c/.../os-note ~/`). Files under `/mnt/c` go through a Windows file-sharing
layer that is slow and doesn't support things like `O_DIRECT` - storage benchmarks there would
measure the wrong thing entirely.

## Running a project

Every C project has the same Makefile shape:

```bash
cd practice/<module>/<project>
make          # build into ./bin
make demo     # run the experiments in order (most projects)
make clean
```

## Projects by module

| Module | Project | Demonstrates |
| --- | --- | --- |
| OS intro | [hello-three-ways](os-into/hello-three-ways) | `printf` vs `write()` vs a raw `syscall` instruction; the hardware via `/proc` & `/sys` |
| Anatomy of a process | [process-memory-map](anatomy-of-process/process-memory-map) | text/data/bss/heap/stack addresses vs `/proc/self/maps`; brk vs mmap; lazy allocation; a leak |
| | [stack-frames](anatomy-of-process/stack-frames) | the compiler's call/ret assembly, frames growing down, stack overflow, dangling pointers, gdb |
| Memory management | [cache-locality](memory-management/cache-locality) | 64-byte cache lines: row vs column traversal, stride sweep |
| | [virtual-memory-lab](memory-management/virtual-memory-lab) | copy-on-write after `fork()`, demand paging, huge pages, the zero page |
| Inside the CPU | [toy-cpu](inside-the-cpu/toy-cpu) | fetch → decode → execute on an 8-instruction machine; memory stalls |
| | [cpu-wait-times](inside-the-cpu/cpu-wait-times) | L1/L2/L3/RAM latency ladder, branch misprediction, instruction-level parallelism |
| Process management | [process-vs-thread](process-management/process-vs-thread) | shared vs copied memory, PID vs TID, creation cost, fork cost vs parent size |
| | [context-switch-cost](process-management/context-switch-cost) | pipe ping-pong: µs per switch, voluntary switch counters |
| | [race-condition-lab](process-management/race-condition-lab) | lost updates; mutex vs atomic vs no sharing; deadlock and lock ordering |
| | [when-threads-help](process-management/when-threads-help) | thread scaling for CPU-bound vs I/O-bound work |
| | [mini-shell](process-management/mini-shell) | a working shell: fork/exec/wait, pipes, dup2, signals, builtins |
| Storage management | [file-io-journey](storage-management/file-io-journey) | page cache, `fsync`, `O_DIRECT`, cold vs warm reads, crash-safe rename |
| | [toy-filesystem](storage-management/toy-filesystem) | an inode file system in a disk image: superblock, bitmaps, inodes, directories, hard links |
| Socket management | [socket-buffers](socket-management/socket-buffers) | send/receive buffers, byte-stream framing, the accept queue and `listen()` backlog |
| | [tcp-server-patterns](socket-management/tcp-server-patterns) | iterative vs thread-per-connection vs epoll event loop, under slow clients |
| OS concepts | [build-pipeline](os-concepts/build-pipeline) | preprocess → compile → assemble → link; symbols, relocations; static vs shared libs |
| | [syscall-cost](os-concepts/syscall-cost) | function call vs syscall vs vDSO, in nanoseconds |
| | [mini-container](os-concepts/mini-container) | namespaces + chroot + cgroups = a container (needs sudo) |
| | [page-table-walker](os-concepts/page-table-walker) | simulated 4-level page walk + TLB; real page-table bloat across processes |
| | [timeouts-lab](os-concepts/timeouts-lab) | connect / read timeouts, TCP keepalive vs a silently dropped connection |
| | [struct-cacheline-layout](os-concepts/struct-cacheline-layout) | hot-field grouping and false sharing (the Google TCP stack story) |
| | [kexec-reboot-sim](os-concepts/kexec-reboot-sim) | copy / verify / decompress in a kexec reboot (the ByteDance reboot story) - Python |

## Honest notes about the environment

- **Numbers are from one laptop** (Intel i5-8365U, 2 cores / 4 threads, WSL2 under Hyper-V).
  Yours will differ. The *ratios and shapes* - which is faster, where the jump happens - are
  what each project is about.
- **WSL2 is a VM.** Anything involving cross-core wake-ups, interrupts or devices pays some
  hypervisor overhead (it shows clearly in `context-switch-cost`'s cross-core mode). Disk
  numbers are a virtual disk on top of the Windows host's SSD.
- **`perf` doesn't work in stock WSL2**, so the projects measure with timers and kernel
  counters (`/proc/self/status`, `getrusage`, `mincore`, `ioctl`) instead of hardware
  performance counters. The Makefiles include `perf` targets for when you run on bare-metal Linux.
- **Two scenarios need `sudo` and weren't run end-to-end while writing:** `mini-container`
  (namespaces/cgroups) and scenario 3 of `timeouts-lab` (iptables). Their code builds
  cleanly; their READMEs say exactly which output is captured and which is expected.
- **Two projects are simulations** by necessity: `page-table-walker/walker` (user space can't
  walk real page tables) and `kexec-reboot-sim` (it times the same copy / SHA-256 / decompress
  work a kexec reboot does, but in user space, not in purgatory). Each README says what is
  measured on the real kernel and what is modeled.
