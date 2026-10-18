# Demo: Process Management

The lecture demo makes the section concrete with real tools: listing processes and threads,
reading their states, watching the scheduler and context switches, and following a process from
`fork` to `exit`. Below is a reproducible version; the lecture's exact steps can be added here
from the `.txt`.

## 1. Who's running?

```bash
ps -ef --forest | head -40          # every process, as a parent/child tree
ps -o pid,ppid,stat,ni,pri,psr,cmd -p $$   # your shell: state, nice, priority, which CPU (psr)
pstree -p | head                    # the same tree, compact
top                                 # press H to toggle threads, 1 for per-CPU lines
```

The `STAT` column:

| Code | Meaning |
| --- | --- |
| `R` | running or runnable (on a run queue) |
| `S` | interruptible sleep - blocked waiting for something (most processes, most of the time) |
| `D` | uninterruptible sleep - usually waiting on disk I/O; can't be killed until it returns |
| `T` | stopped (Ctrl+Z, a debugger) |
| `Z` | zombie - exited, waiting for its parent to `wait()` |
| `+`, `s`, `l`, `<`, `N` | foreground group, session leader, multi-threaded, high / low priority |

## 2. Threads are tasks too

```bash
ps -eLf | head                      # one line per thread; LWP = thread ID
ls /proc/<pid>/task/                # one directory per thread
grep Threads /proc/<pid>/status
```

## 3. The life of a process

```bash
sleep 30 &                          # fork + exec, in the background
ps -o pid,ppid,stat,cmd -p $!       # S - sleeping
kill -STOP $!; ps -o stat -p $!     # T - stopped
kill -CONT $!; kill $!              # SIGTERM: ask it to exit
wait $!; echo "exit status: $?"     # 143 = 128 + 15 (killed by SIGTERM)
```

Watching it from the kernel's side: `strace -f -e trace=process bash -c 'ls | wc -l'` shows the
`clone`, `execve`, `wait4` and `exit_group` calls a shell makes for a pipeline.

## 4. Scheduling and priority

```bash
nice -n 19 ./cpu-hog &              # lowest priority (nice 19)
./cpu-hog &                         # normal priority (nice 0)
top                                 # with both on one CPU, the nice-0 one gets most of the time
taskset -c 0 ./cpu-hog              # pin to CPU 0
chrt -p <pid>                       # scheduling policy: SCHED_OTHER, SCHED_FIFO, SCHED_RR...
```

## 5. Context switches

```bash
vmstat 1                            # 'cs' = context switches/sec, 'r' = runnable threads
grep ctxt /proc/<pid>/status        # voluntary vs nonvoluntary
```

🖼️ **reading ps and top:**
![reading ps and top](<../../assets/img/ps-top-annotated.png>)

## 6. Build it yourself: a shell

The best demo of process management is writing the program that does it all day:

```bash
cd practice/process-management/mini-shell && make demo
```

```text
[msh] fork() -> child pid 614 will exec "echo"
[msh] fork() -> child pid 615 will exec "tr"
[msh] fork() -> child pid 616 will exec "wc"
[msh] waitpid(614) -> exited with 0
[msh] waitpid(615) -> exited with 0
3
[msh] waitpid(616) -> exited with 0
```

A pipeline is three processes started together and wired with pipes; `cd` is a builtin because
the working directory belongs to the shell process itself; a missing command exits 127.

## What to take away

- **Most processes are asleep** (`S`) most of the time - blocked, using no CPU.
- **Every process has a parent** (except PID 1) and is cleaned up by its `wait()`.
- **Threads show up as tasks** with their own IDs, sharing one PID.
- **`nice` and affinity** are the user-visible knobs on the scheduler.

## Practice projects

- [`practice/process-management/mini-shell`](../../practice/process-management/mini-shell) -
  fork/exec/wait, pipes, signals, builtins, background jobs.
- [`practice/process-management/process-vs-thread`](../../practice/process-management/process-vs-thread) -
  PID vs TID, shared vs copied memory.
- [`practice/process-management/context-switch-cost`](../../practice/process-management/context-switch-cost) -
  the switch counters from step 5, measured.

## Related

- [Process management](process-management.md) - the section overview.
- [Program vs process](<../anatomy of process/program-vs-process.md>) - the lifecycle states.
- [Context switching](context-switching.md) - what `cs` in `vmstat` is counting.
