# Practice: TCP Server Patterns

Concept notes:
[`socket-programming-patterns.md`](<../../../concepts/socket management/socket-programming-patterns.md>) ·
[`asynchronous-io.md`](<../../../concepts/socket management/asynchronous-io.md>) ·
[`socket-management-demo.md`](<../../../concepts/socket management/socket-management-demo.md>)

## What this demonstrates

The same tiny echo protocol served three ways, then hit with 10 **slow clients** at once:

| Server | Model | What blocks? |
| --- | --- | --- |
| `iterative` | one thread, blocking calls | the whole server, on whichever client it's talking to |
| `threaded` | one thread per connection | only that client's thread |
| `epoll` | one thread, non-blocking sockets, event loop | nothing except `epoll_wait()` |

The protocol: the server greets with `hi\n`, the client "thinks" for 500ms, sends a line, and
gets `echo: <line>` back.

## Folder structure

```text
tcp-server-patterns/
  common.h      <- listen_on(), and the blocking protocol used by iterative + threaded
  iterative.c   <- accept() -> serve -> accept() -> serve ...
  threaded.c    <- accept() -> pthread_create() per connection
  epoll.c       <- epoll event loop, per-connection read buffers (the reactor pattern)
  loadgen.c     <- N concurrent slow clients, prints total time
  Makefile      <- `make`, `make demo` (starts each server, runs loadgen, stops it)
```

## Build & run

```bash
make demo

# or by hand, in two terminals:
./bin/epoll 9203
./bin/loadgen 9203 10 500     # port, clients, think time (ms)
```

## What you'll actually see

```text
  => 10 clients x 500ms think time, all served in 5006 ms     <- iterative
  => 10 clients x 500ms think time, all served in 502 ms      <- threaded
  => 10 clients x 500ms think time, all served in 501 ms      <- epoll
```

- **Iterative: 10 × 500ms = 5 seconds.** While it waits for client 1 to finish thinking, it
  isn't calling `accept()` - clients 2-10 sit in the kernel's accept queue, and none of them
  gets a greeting until the server reaches it.
- **Threaded and epoll: ~500ms**, the time of one client. Both overlap all the waiting - one
  by giving every client its own blocked thread, the other by never blocking on any one client.
- At 10 clients they look identical. The difference shows at **10,000 clients**: 10,000 threads
  (each with a stack and a scheduler entry) vs 1 thread and an `epoll` set.

**Why the greeting matters**: without it, all three servers looked equally fast. The clients'
lines arrived in the **kernel's** receive buffers while the iterative server was busy, so when
it got to each client the data was already waiting. The kernel was doing concurrent I/O on the
app's behalf - see [socket-buffers](../socket-buffers). The greeting makes each client wait for
the *application* to pay attention, which is what a real request/response exchange does.

## Code walkthrough

**Iterative** - the simplest possible server, and why it doesn't scale:

```c
for (;;) {
    int cfd = accept(lfd, NULL, NULL);  // blocks until a connection is queued
    serve_one(cfd);                     // blocks until THIS client is done
}
```

**Threaded** - move the blocking into a thread per client:

```c
int cfd = accept(lfd, NULL, NULL);
pthread_create(&th, NULL, client_thread, (void *)(long)cfd);
pthread_detach(th);
```

**epoll** - one thread asks the kernel "which sockets are ready?" and only touches those:

```c
int n = epoll_wait(ep, ready, 64, -1);         // the ONLY place this thread ever blocks
for (int i = 0; i < n; i++) {
    int fd = ready[i].data.fd;
    if (fd == lfd) { /* accept every queued connection, set non-blocking, add to epoll */ }
    else {
        ssize_t r = read(fd, bufs[fd] + used[fd], ...);   // whatever is there, never waits
        if (r < 0 && errno == EAGAIN) continue;
        // no full line yet? keep the partial data in bufs[fd] and go back to epoll_wait()
    }
}
```

The price of epoll is **state**: a blocking server keeps "where am I with this client" on the
thread's stack for free; an event loop has to store it explicitly (`bufs[fd]`, `used[fd]`)
because it returns to `epoll_wait` between every chunk of data. That's the job async/await
and callbacks do in higher-level languages.

## Try this yourself

- `./bin/loadgen <port> 200 500` against `threaded` and `epoll`, and watch
  `grep Threads /proc/$(pgrep threaded)/status` while it runs.
- Add a CPU-heavy step (e.g. a 50ms busy loop) to request handling in `epoll.c`. Now the event
  loop *is* blocked while it computes, and it degrades to iterative behavior. That's why event
  loops push CPU work to a worker pool.
- Make `epoll.c` handle large replies properly: when `write()` returns less than requested,
  store the rest, switch the fd to `EPOLLOUT`, and finish writing when it becomes writable.
- Try **edge-triggered** mode (`EPOLLIN | EPOLLET`): you must now read until `EAGAIN` every
  time, or you'll never be notified about the leftover data.

## Real-world equivalent

| Pattern | Used by |
| --- | --- |
| Iterative | toy servers, some single-client daemons |
| Thread / process per connection | Apache prefork & worker MPMs, PostgreSQL (process), MySQL (thread), classic Java servlet containers |
| Event loop (reactor) | nginx, Redis, Node.js (libuv), HAProxy, Netty, Python asyncio |
| Event loop per core | nginx workers, Envoy, Seastar/ScyllaDB - often with `SO_REUSEPORT` |
| Goroutines / virtual threads | Go, Java 21 virtual threads - *write* blocking code, but the runtime runs it on an epoll-based scheduler underneath |
