# Practice: Socket Buffers & Kernel Queues

Concept notes:
[`sockets-connections-n-kernel-queues.md`](<../../../concepts/socket management/sockets-connections-n-kernel-queues.md>) ·
[`sending-n-receiving-data.md`](<../../../concepts/socket management/sending-n-receiving-data.md>) ·
[`socket-management-demo.md`](<../../../concepts/socket management/socket-management-demo.md>)

## What this demonstrates

The kernel does a surprising amount of networking **on the application's behalf**, in queues
the application never sees directly. Both programs here are single, self-contained runs -
no second terminal needed.

1. **`buffers`** - `send()` doesn't send anything to the network; it copies bytes into the
   kernel's **send buffer** and returns. With a receiver that never calls `recv()`, the sender
   can still "send" megabytes before it finally blocks. You'll see exactly where those bytes
   are sitting, using the kernel's own counters.
2. **`buffers`, part 2** - TCP is a **byte stream**: three `send()` calls can arrive as one
   `recv()`. Message boundaries are your problem.
3. **`backlog`** - the **accept queue**: a server calls `listen(fd, 2)` and never calls
   `accept()`. The kernel still completes TCP handshakes for the first few clients, then
   starts dropping SYNs.

## Folder structure

```text
socket-buffers/
  buffers.c   <- send/receive buffers, SIOCOUTQ / SIOCINQ, the byte-stream experiment
  backlog.c   <- listen(fd, 2) + 8 clients + `ss` showing the accept queue
  Makefile    <- `make`, `make demo`, `make limits` (system-wide sysctls)
```

## Build & run

```bash
make demo
make limits    # somaxconn, syn backlog, tcp_rmem / tcp_wmem
```

## What you'll actually see

```text
1) a receiver that never calls recv()
   client SO_SNDBUF = 2565 KB, server SO_RCVBUF = 128 KB
   send() accepted 2493 KB before returning EAGAIN - and the server read NOTHING
   where the bytes are sitting right now (all inside the kernel):
     client send queue   (SIOCOUTQ):   2429 KB  - sent, waiting for the peer
     server receive queue (SIOCINQ):     63 KB  - arrived, waiting for recv()
   server finally recv()s 63 KB -> window opens -> client send() works again: 16384 bytes

2) TCP is a byte stream - it does not keep your message boundaries
   client: send("A"), send("BB"), send("CCC")  (3 calls)
   server: one recv() returned 6 bytes: "ABBCCC"
   -> real protocols add framing: a length prefix, or a delimiter like \r\n

server: listen(fd, 2) on 127.0.0.1:9100 - and never calls accept()

client 1: connected      (handshake done, sitting in the accept queue)
client 2: connected      (handshake done, sitting in the accept queue)
client 3: connected      (handshake done, sitting in the accept queue)
client 4: still waiting  (SYN dropped - accept queue full, kernel will retry)
...
client 8: still waiting  (SYN dropped - accept queue full, kernel will retry)

the kernel's view (Recv-Q = connections queued, Send-Q = queue limit):
State  Recv-Q Send-Q Local Address:Port Peer Address:PortProcess
LISTEN 3      2          127.0.0.1:9100      0.0.0.0:*

server: now calling accept() in a loop...
server: accept() returned 3 connections, then EAGAIN (queue empty)
```

- **~2.4MB "sent" to a peer that read nothing.** `send()` succeeding only ever meant "the
  kernel has your bytes". The receiver's queue holds 63KB - its advertised **receive window**
  was full, so TCP **flow control** stopped the sender, and the rest piled up in the sender's
  own buffer. (Loopback auto-tunes the send buffer very large; across a real network it's
  usually smaller.)
- **3 connections for a backlog of 2** - Linux lets the accept queue hold `backlog + 1`. The
  `ss` output shows it directly: for a listening socket, `Recv-Q` is the current queue length
  and `Send-Q` is the configured limit.
- **Clients 4-8 didn't get "connection refused"** - their SYNs were silently dropped, so their
  kernels will retransmit (after 1s, 3s, 7s...). From the client side, an overloaded server
  looks like a slow network.

## Code walkthrough

```c
fcntl(c, F_SETFL, O_NONBLOCK);       // when buffers are full, fail with EAGAIN instead of sleeping
for (;;) {
    ssize_t n = send(c, chunk, sizeof chunk, 0);
    if (n > 0) { total += n; continue; }
    if (errno == EAGAIN) break;      // a blocking socket would sleep right here
}
ioctl(c, SIOCOUTQ, &n);              // bytes in the send queue, not yet ACKed by the peer
ioctl(s, SIOCINQ, &n);               // bytes in the receive queue, not yet read by the app
```

```c
listen(lfd, BACKLOG);                // creates the queue; the kernel handles handshakes now
connect(fds[i], ...);                // non-blocking: returns EINPROGRESS immediately
poll(&p, 1, 1500);                   // writable = handshake completed (or failed: check SO_ERROR)
```

## Try this yourself

- In `buffers.c`, `setsockopt(s, SOL_SOCKET, SO_RCVBUF, ...)` to 4KB before connecting, and
  watch how much less the server's queue holds (the kernel doubles the value you ask for, to
  leave room for bookkeeping - `getsockopt` will show 8KB).
- In `backlog.c`, raise `BACKLOG` to 6 - now 7 clients connect. Then check
  `sysctl net.core.somaxconn`: any backlog above it is silently capped.
- While `backlog` is waiting, run `ss -tn state syn-sent` to see the clients stuck mid-handshake.
- `nstat -az | grep -i -E 'ListenOverflows|ListenDrops'` counts every dropped SYN - the
  metric to alert on in production.

## Real-world equivalent

- **`ListenOverflows`** is a classic production alert: the app isn't calling `accept()` fast
  enough (blocked event loop, too few acceptor threads), and clients see timeouts that look
  like network problems. Nginx's `listen ... backlog=`, Node's `server.listen(port, backlog)`
  and `net.core.somaxconn` exist for this queue.
- The byte-stream lesson is why every protocol on top of TCP defines **framing**: HTTP/1.1
  uses `\r\n` and `Content-Length`, HTTP/2 and gRPC use length-prefixed frames, Redis uses
  `\r\n`-terminated lines with length prefixes.
- Huge send queues piling up behind a slow reader is **bufferbloat** at the socket level - and
  why proxies cap per-connection buffers.
