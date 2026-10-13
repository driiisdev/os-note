# Practice: Mini Shell (`msh`)

Concept notes:
[`demo-process-management.md`](<../../../concepts/process management/demo-process-management.md>) ·
[`process-management.md`](<../../../concepts/process management/process-management.md>) ·
[`program-vs-process.md`](<../../../concepts/anatomy of process/program-vs-process.md>)

## What this demonstrates

A working shell in ~170 lines of C - the capstone for process management, because a shell is
nothing *but* process management:

- **`fork()`** to create a child, **`execvp()`** to turn it into another program,
  **`waitpid()`** to collect its exit status (and stop it becoming a zombie).
- **Pipes**: `cmd1 | cmd2 | cmd3` is N processes whose stdout/stdin are wired together with
  `pipe()` and `dup2()` - the processes never know.
- **Signals**: Ctrl+C kills the foreground command, not the shell.
- **Builtins**: why `cd` *can't* be a separate program.
- **Background jobs** (`&`) and reaping them with `WNOHANG`.

## Folder structure

```text
mini-shell/
  msh.c       <- parser, builtins, pipeline runner, main loop
  Makefile    <- `make`, `make demo` (pipes a script through the shell with tracing on)
```

## Build & run

```bash
make
./bin/msh                  # interactive: try `ls -l | grep c | wc -l`, `sleep 5 &`, Ctrl+C
MSH_TRACE=1 ./bin/msh      # print every fork / exec / wait the shell performs
make demo
```

## What you'll actually see

```text
$ make demo
[msh] fork() -> child pid 613 will exec "echo"
hello from msh
[msh] waitpid(613) -> exited with 0
[msh] fork() -> child pid 614 will exec "echo"
[msh] fork() -> child pid 615 will exec "tr"
[msh] fork() -> child pid 616 will exec "wc"
[msh] waitpid(614) -> exited with 0
[msh] waitpid(615) -> exited with 0
3
[msh] waitpid(616) -> exited with 0
[msh] fork() -> child pid 617 will exec "ls"
ls: cannot access '/does-not-exist': No such file or directory
[msh] waitpid(617) -> exited with 2
[msh] fork() -> child pid 618 will exec "nope"
msh: nope: No such file or directory
[msh] waitpid(618) -> exited with 127
[msh] fork() -> child pid 619 will exec "pwd"
/tmp
[msh] waitpid(619) -> exited with 0
```

- A three-stage pipeline is **three processes started together** (614, 615, 616), running
  concurrently - not one after another.
- `nope` doesn't exist: `fork()` succeeded, `execvp()` failed, and the child exits with
  **127** - the same code bash uses for "command not found".
- `cd /tmp` produced **no fork** - it's a builtin - and the later `pwd` (a child) inherited the
  new working directory.

## Code walkthrough

The heart of it, for each command in the pipeline:

```c
int fd[2];
if (i < n - 1) pipe(fd);                 // a pipe to the NEXT command

pid_t pid = fork();
if (pid == 0) {                          // ---- child ----
    signal(SIGINT, SIG_DFL);             // Ctrl+C should kill commands...
    if (prev_read != -1) dup2(prev_read, STDIN_FILENO);   // stdin  <- previous pipe
    if (fd[1] != -1)     dup2(fd[1], STDOUT_FILENO);      // stdout -> next pipe
    execvp(cmds[i][0], cmds[i]);         // replace this process's program; never returns
    _exit(127);                          // ...unless it failed
}
// ---- parent ----
if (prev_read != -1) close(prev_read);
if (fd[1] != -1) close(fd[1]);           // if the shell kept this open, the reader never sees EOF
prev_read = fd[0];
```

```c
signal(SIGINT, SIG_IGN);                 // ...but not the shell itself
```

- **`dup2(a, b)`** makes file descriptor `b` refer to whatever `a` refers to. After
  `dup2(fd[1], 1)`, anything the program writes to "stdout" goes into the pipe.
- **Closing unused pipe ends matters.** A reader only gets EOF when *every* write end is
  closed - including stray copies in the shell. Forget one and `wc` waits forever.
- **`cd` must be a builtin**: the working directory is per-process state. A child changing its
  own directory and then exiting changes nothing in the shell.
- **Zombies**: a finished child stays in the process table (state `Z`) until its parent calls
  `wait`. The `WNOHANG` loop at the top of the prompt reaps finished background jobs.

## Try this yourself

- Run `sleep 30 &` in `msh`, then `ps -o pid,ppid,stat,cmd` from another terminal. Remove the
  `WNOHANG` reaping loop, rebuild, and watch the finished `sleep` linger as `Z` (zombie).
- Add **output redirection**: `cmd > file` is `open()` + `dup2(fd, STDOUT_FILENO)` in the child.
- Add `$?` expansion using the `status` variable.
- Add **job control** (`fg`, Ctrl+Z): you'll need process groups (`setpgid`, `tcsetpgrp`) and
  `SIGTSTP` - the part of a real shell that's genuinely hard.

## Real-world equivalent

Bash, zsh and fish do exactly this (plus job control, globbing, and a real grammar). So does
every program that launches others: `systemd` starting services, Docker's `runc` starting a
container's first process, Python's `subprocess.Popen`, Node's `child_process.spawn` - all are
`fork`/`exec` (or `posix_spawn`/`clone`) plus `dup2` for stdio and `wait` for the exit status.
