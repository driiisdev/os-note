# Sending and Receiving Data

`send()` and `recv()` don't talk to the network. They **copy bytes between your buffer and the
kernel's socket buffers**. Everything that actually crosses the wire - segmentation, ACKs,
retransmission, flow control - happens in the kernel, on its own schedule, often while your
program is doing something else.

## Sending

```text
 app: send(fd, buf, 10000)
   |
   v  copy 10000 bytes into the socket's SEND BUFFER     <- send() returns here
 kernel TCP:
   - cut the byte stream into segments (<= MSS, ~1448 bytes on Ethernet)
   - send as many as the receiver's window and the congestion window allow
   - keep every byte in the send buffer until it's ACKed (it may need resending)
   - retransmit on timeout or duplicate ACKs
 NIC driver: place packets on the TX ring -> NIC DMAs them out of RAM -> wire
```

- **`send()` returns when the data is in the kernel**, not when it's delivered. A successful
  `send()` followed by the peer crashing means the data is simply lost.
- **If the send buffer is full**, a blocking `send()` sleeps until ACKs free up space; a
  non-blocking one returns `EAGAIN`.
- **Partial writes**: `send()` may accept fewer bytes than you asked for - always check the
  return value and loop.
- **Nagle's algorithm** holds back small segments while earlier data is unacknowledged, to
  avoid flooding the network with tiny packets. Interactive protocols disable it with
  `TCP_NODELAY`.

## Receiving

```text
 wire -> NIC: DMA the frame into an RX ring buffer in RAM, raise an interrupt
 kernel: (softirq / NAPI polling) parse Ethernet -> IP -> TCP
         check sequence numbers, put payload in the socket's RECEIVE BUFFER in order
         send an ACK (possibly delayed, to piggy-back)
         wake any thread blocked in recv() / epoll_wait() on this socket
 app: recv(fd, buf, 4096)   <- copies whatever is buffered (up to 4096 bytes) and returns
```

- **If the receive buffer is empty**, a blocking `recv()` sleeps; non-blocking returns `EAGAIN`.
- **`recv()` returning 0** means the peer closed its side (FIN received) - end of stream.
- **Flow control**: every ACK advertises how much free space is left in the receive buffer
  (the **window**). If the application stops reading, the window shrinks to zero and the
  **sender** stops - its own send buffer fills and its `send()` calls block. Backpressure,
  all the way from a slow reader to the writer.

🖼️ **send and receive path:**
![send and receive](<../../assets/img/send-receive-path.png>)

## TCP is a byte stream, not a message stream

```c
send(fd, "A", 1, 0);
send(fd, "BB", 2, 0);
send(fd, "CCC", 3, 0);
// the receiver's recv() might return "ABBCCC" in one call - or "AB", then "BCCC"...
```

TCP preserves the **order** and **content** of bytes, but not the boundaries between `send()`
calls. Every protocol on top of TCP therefore defines **framing**:

- a **delimiter** - HTTP/1.1 headers end with `\r\n\r\n`, Redis commands end with `\r\n`;
- a **length prefix** - HTTP/2 frames, gRPC messages, most binary protocols;
- a **fixed size** - some simple binary protocols.

A reader must keep reading and buffering until it has a complete message. (UDP is the opposite:
each `recvfrom()` returns exactly one datagram - boundaries preserved, delivery not guaranteed.)

## Copies, and avoiding them

A plain `read(file)` + `send(socket)` copies the data twice through user space (page cache → user
buffer → socket buffer). Zero-copy options:

- **`sendfile()`** - page cache → socket directly (how nginx serves static files);
- **`splice()`** - move data between file descriptors through a pipe without user-space copies;
- **`MSG_ZEROCOPY`** - the NIC DMAs directly from your pages; you're notified when it's safe to reuse them.

## Buffer sizes

```bash
sysctl net.ipv4.tcp_rmem net.ipv4.tcp_wmem    # min / default / max per-socket buffer, bytes
```

Linux **auto-tunes** buffer sizes per connection up to the max. Fast, long-distance links need big
buffers: to keep a 1Gbit/s link with 100ms RTT full, ~12.5MB must be "in flight" (the
bandwidth-delay product).

## Clarifications

- **"Sent" means "handed to the kernel".** The only proof of delivery is an application-level
  reply.
- **`recv()` doesn't wait for a full buffer** - it returns as soon as *any* data is available.
- **A large `Send-Q` in `ss`** means the peer (or the network) is slow; a large **`Recv-Q`** means
  *your* application isn't reading fast enough.

## Practice project

[`practice/socket-management/socket-buffers`](../../practice/socket-management/socket-buffers) -
`buffers` sends ~2.4MB to a peer that never reads before `send()` returns `EAGAIN`, then shows
where those bytes sit (`SIOCOUTQ` on the sender, `SIOCINQ` on the receiver); after one `recv()`
frees space, `send()` works again. Part 2 shows three `send()`s arriving as one 6-byte `recv()`.

## Related

- [Sockets, connections and kernel queues](sockets-connections-n-kernel-queues.md) - the socket
  these buffers belong to.
- [DMA](<../memory management/dma.md>) - the NIC side of the path.
- [Asynchronous I/O](asynchronous-io.md) - how to wait for "readable"/"writable" without blocking.
