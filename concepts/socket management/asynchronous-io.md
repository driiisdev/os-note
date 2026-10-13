# Asynchronous I/O

Every I/O operation has a waiting part - for a packet, for the disk, for the other side to read.
The question is **who waits, and how**. "Asynchronous I/O" covers a spectrum of answers, from a
thread that just sleeps, to a kernel that does the whole operation and tells you when it's done.

## The four I/O models

### 1. Blocking I/O

```c
n = read(fd, buf, sizeof buf);   // the thread sleeps until data is there
```

Simplest code. One thread can only wait on one thing at a time, so concurrency = more threads.

### 2. Non-blocking I/O

```c
fcntl(fd, F_SETFL, O_NONBLOCK);
n = read(fd, buf, sizeof buf);   // returns immediately: data, or -1 with errno = EAGAIN
```

Never sleeps - but on its own you'd have to **busy-poll** every socket in a loop, burning CPU.
Useful only combined with model 3.

### 3. I/O multiplexing (readiness-based): `select` / `poll` / `epoll` / `kqueue`

```c
int n = epoll_wait(ep, events, 64, -1);   // sleep until ANY registered fd is ready
for (i = 0; i < n; i++)
    read(events[i].data.fd, ...);         // non-blocking read - guaranteed not to block (much)
```

One thread waits on **thousands of fds at once**. The kernel answers "these are ready"; the
application then does the actual read/write itself.

| API | How it scales | Notes |
| --- | --- | --- |
| `select` | O(n) per call, max 1024 fds (`FD_SETSIZE`) | portable, ancient |
| `poll` | O(n) per call, no fd limit | still re-scans the whole list each time |
| `epoll` (Linux) | O(ready) - the kernel keeps the interest list | level- or edge-triggered |
| `kqueue` (BSD/macOS) | O(ready) | also handles timers, signals, processes |

**Level-triggered** (default): `epoll_wait` keeps reporting the fd as long as data remains.
**Edge-triggered** (`EPOLLET`): reported once per new arrival - you must read until `EAGAIN` or
you'll never be told about the leftovers.

### 4. Completion-based (true async): `io_uring`, Windows IOCP, POSIX AIO

```text
 app:    "read 4KB from fd 7 into buf, tell me when done"  -> submission queue (shared memory)
 kernel: performs the read (or the device DMAs into buf)
 app:    later, picks up the result from the completion queue (shared memory)
```

The kernel does the **whole operation**, including the copy, and notifies you on completion.

- **Windows IOCP** has worked this way for decades.
- **Linux `io_uring`** (2019+): two ring buffers shared between the app and the kernel - a
  submission queue and a completion queue - so many operations can be submitted and reaped with
  one syscall, or with none at all in polling mode. Works for files, sockets, `accept`, `open`,
  `fsync`...
- **POSIX AIO** (`aio_read`) on Linux is limited (glibc implements it with threads) and rarely used.

🖼️ **Image to add - I/O models compared:** four timelines (blocking, non-blocking + busy poll,
epoll, io_uring), each showing the application thread and the kernel across "wait for data" and
"copy data" phases - who is blocked when, and how many syscalls happen.
![I/O models](<../../assets/img/io-models.png>)

## Readiness vs completion: the file problem

Readiness APIs answer "can I read without blocking?". For **sockets and pipes** that's a
meaningful question. For **regular files**, the answer is always "yes" - even if the read will
then block for 10ms waiting on the disk. That's why:

- `epoll` can't make disk I/O non-blocking;
- **Node.js / libuv** runs file operations on a **thread pool** while using epoll for sockets;
- **io_uring** matters so much: it's the first Linux interface that makes *file* I/O genuinely
  asynchronous at scale.

## What async buys, and what it costs

- **Buys**: very high concurrency on few threads, low memory per connection, fewer context switches.
- **Costs**: code complexity (state machines, callbacks, futures, async/await), and the rule that
  **nothing on the event loop may block** - one slow synchronous call stalls everything.

Language features hide the state machine: JavaScript promises + `async/await`, Python `asyncio`,
Rust `async` + Tokio (epoll/io_uring), C# `async` (IOCP), Kotlin coroutines, Go goroutines (the
runtime hides even the async-ness).

## Clarifications

- **Non-blocking ≠ asynchronous.** Non-blocking means "don't make me wait, tell me to try again";
  asynchronous (completion) means "do it and tell me when it's finished".
- **"Async" code still waits somewhere** - in `epoll_wait` or on a completion queue - but only in
  one place, for everything at once.
- **Async doesn't make the device faster.** It lets one thread overlap many waits instead of
  dedicating a thread to each.

## Practice project

[`practice/socket-management/tcp-server-patterns`](../../practice/socket-management/tcp-server-patterns) -
`epoll.c` is a complete readiness-based (model 3) server: non-blocking sockets, `epoll_wait` as the
only blocking call, per-connection buffers for partial reads. Its "try this" section covers
edge-triggered mode and proper `EPOLLOUT` handling for partial writes.

## Related

- [Socket programming patterns](socket-programming-patterns.md) - the event-loop server pattern
  built on this.
- [What really happens in a file I/O?](<../storage management/what-really-happens-in-a-file-io.md>) -
  why file reads block.
- [Kernel vs user mode switching](<../os concepts/kernel-vs-user-mode-switching.md>) - io_uring's
  main win is fewer of these.
