# Socket Programming Patterns

Every server answers the same question: **while one client is slow, what happens to everyone
else?** The answer depends on how the server maps connections onto processes and threads, and on
how it waits for I/O. A handful of patterns cover almost every real server.

## 1. Iterative (single-threaded, blocking)

```c
for (;;) {
    int c = accept(lfd, NULL, NULL);   // wait for a connection
    handle(c);                         // read request, compute, write response, close
}
```

- One client at a time. While `handle()` waits on a slow client's `read()`, nobody else is served;
  they queue in the kernel's accept queue.
- Fine for tools and tests; useless for a real service.

## 2. Process per connection (`fork`)

```c
int c = accept(lfd, NULL, NULL);
if (fork() == 0) { close(lfd); handle(c); _exit(0); }
close(c);
```

- Strong isolation: a crash or memory leak only affects one connection.
- Expensive to create; heavy on memory; no shared in-memory state without IPC.
- Used by: classic `inetd`, Apache **prefork**, **PostgreSQL** (a backend process per
  connection, hence PgBouncer), sshd.

## 3. Thread per connection

```c
int c = accept(lfd, NULL, NULL);
pthread_create(&t, NULL, handle_thread, (void *)(long)c);
```

- Simple, blocking code; cheap to share state (carefully).
- Each thread costs a stack (MBs of virtual, tens of KB resident) and a scheduler entry; at tens
  of thousands of connections, memory and [context switches](<../process management/context-switching.md>)
  dominate - the **C10K problem**.
- Used by: MySQL, classic Java servlet containers, Apache **worker** MPM.

## 4. Thread pool

The acceptor hands connections (or requests) to a fixed set of workers through a queue.
Bounded resource use, but a pool of N threads blocked on N slow clients is exactly as stuck as
pattern 1 with N = 1.

## 5. Event loop (reactor) - non-blocking I/O + multiplexing

```c
set_nonblocking(lfd); epoll_ctl(ep, EPOLL_CTL_ADD, lfd, ...);
for (;;) {
    n = epoll_wait(ep, events, 64, -1);         // "which sockets are ready?"
    for each ready fd:
        if fd == lfd: accept all pending, set non-blocking, add to epoll
        else:         read what's there; if a full request is buffered, handle it and write
}
```

- **One thread handles thousands of connections**, because it never blocks on any single one -
  it only touches sockets the kernel reports as ready.
- The cost is **explicit state**: partial reads and unsent writes must be stored per connection,
  because the thread returns to the loop between every chunk (callbacks, promises and async/await
  exist to make this bearable).
- **Any CPU-heavy or blocking call stalls every connection** on that loop - such work must go to a
  worker thread pool.
- Used by: **nginx**, **Redis**, **Node.js** (libuv), HAProxy, Netty, Python asyncio.

🖼️ **server patterns compared:**
![server patterns](<../../assets/img/server-patterns.png>)

## 6. Multiple event loops (one per core)

One event loop can only use one core. To use all of them:

- **one acceptor, N worker loops** - the acceptor hands each new connection to a worker loop
  (Netty's boss/worker groups, libuv-based servers);
- **N loops, each with its own listening socket on the same port via `SO_REUSEPORT`** - the
  kernel spreads incoming connections across their accept queues (nginx `reuseport`, Envoy);
- **N processes sharing one listening socket** - nginx's classic worker processes.

## 7. Lightweight threads (green threads / goroutines / virtual threads)

Write blocking-style code (pattern 3), but the language runtime multiplexes millions of cheap
user-space threads onto a few OS threads and an epoll/kqueue event loop underneath. Go's
goroutines and Java 21's virtual threads give pattern-3 simplicity with pattern-5 scalability.

## Comparing them

| Pattern | Concurrency limit | Isolation | Code style | CPU-heavy work |
| --- | --- | --- | --- | --- |
| iterative | 1 | - | blocking, trivial | blocks everyone |
| process per connection | ~hundreds-thousands | strong | blocking | parallel, isolated |
| thread per connection | ~thousands-tens of thousands | weak | blocking + locks | parallel |
| event loop | ~hundreds of thousands per loop | weak | callbacks / async | must be offloaded |
| loop per core | the above × cores | weak | async | must be offloaded |
| green threads | ~millions | weak | blocking-style | runtime schedules it |

## Clarifications

- **Event loops aren't faster per request** - they're cheaper per *connection*. With few
  connections and CPU-heavy requests, threads can win.
- **"Single-threaded" Node.js isn't single-threaded underneath** - libuv uses a thread pool for
  file I/O, DNS and crypto, because those don't have good non-blocking kernel interfaces.
- **The kernel does a lot of the concurrency for you** - handshakes and buffering happen in
  parallel no matter which pattern the app uses (see the practice project's note on why the
  greeting was needed to expose the difference).

## Practice project

[`practice/socket-management/tcp-server-patterns`](../../practice/socket-management/tcp-server-patterns) -
the same echo protocol served iteratively, with a thread per connection, and with an epoll event
loop; 10 clients that each think for 500ms take **5,006ms** on the iterative server and **~500ms**
on the other two.

## Related

- [Asynchronous I/O](asynchronous-io.md) - the kernel interfaces behind pattern 5.
- [Process vs thread](<../process management/process-vs-thread.md>) - the trade-offs behind
  patterns 2 and 3.
- [When do you use threads?](<../process management/when-do-you-use-threads.md>) - sizing pools.
- [Sockets, connections and kernel queues](sockets-connections-n-kernel-queues.md) - the accept
  queue every pattern drains.
