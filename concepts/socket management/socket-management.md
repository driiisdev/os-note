# Socket Management - Section Intro

To the application, a network connection is a **socket** - a file descriptor you `read()` and
`write()`. To the kernel, it's a large amount of machinery: the TCP state machine, queues of
pending connections, send and receive buffers, timers for retransmission, and the drivers that
move packets to and from the NIC. This section is about that machinery, and about how servers
are structured to use it efficiently.

## The picture

```text
 application        accept()          read()/recv()         write()/send()
     |                 ^                    ^                     |
 ----|-----------------|--------------------|---------------------|------ syscalls
     |    listening socket            connection socket            |
     |    +-------------------+       +-------------------------+  |
     |    | SYN queue         |       | receive buffer  ^       |  v
     |    | accept queue  ----+--->   | send buffer     |  -----+--+
     |    +-------------------+       +-------------------------+
 kernel        TCP/IP stack: handshakes, ACKs, retransmits, flow + congestion control
                      ^                                          |
                      |   NIC driver: RX ring <-- DMA     TX ring --> DMA
 ---------------------|------------------------------------------|----- hardware
                    [ NIC ] <==================== wire ==================>
```

🖼️ **Image to add - socket internals:** the diagram above as a clean graphic: application at the
top, a listening socket with SYN and accept queues, a connection socket with send and receive
buffers, the TCP/IP stack, the NIC's DMA rings, and the wire - with arrows for an incoming
packet's path up to `recv()`.
![socket internals](<../../assets/img/socket-internals.png>)

## What this section covers

| # | Topic | The question it answers |
| --- | --- | --- |
| 1 | [Sockets, connections and kernel queues](sockets-connections-n-kernel-queues.md) | What is a socket, and what queues does the kernel keep for it? |
| 2 | [Sending and receiving data](sending-n-receiving-data.md) | What do `send()` and `recv()` actually do? Where do the bytes wait? |
| 3 | [Socket programming patterns](socket-programming-patterns.md) | How do servers handle many connections: processes, threads, event loops? |
| 4 | [Asynchronous I/O](asynchronous-io.md) | Blocking vs non-blocking vs multiplexed vs completion-based I/O |
| 5 | [Socket management demo](socket-management-demo.md) | Watching sockets, queues and buffers with `ss` and friends |

## Networking in one paragraph

Applications exchange **bytes** over **TCP** (reliable, ordered, connection-oriented byte
streams) or **messages** over **UDP** (unreliable datagrams). TCP connections are identified by
a 4-tuple - (source IP, source port, destination IP, destination port) - and are set up with a
three-way handshake (SYN, SYN-ACK, ACK). Everything below the socket API - segmenting the byte
stream into packets, acknowledging, retransmitting, pacing - is done by the kernel. (The
protocol side is covered in depth in the backend-engineering notes; this section focuses on what
the OS does with it.)

## Key ideas to carry forward

- **A socket is a kernel object behind a file descriptor**, so it works with `read`, `write`,
  `close`, `poll` - and counts against the file descriptor limit (`ulimit -n`).
- **The kernel does a lot without the app**: completing handshakes, buffering incoming data,
  sending ACKs, retransmitting lost packets.
- **`send()` returning means "the kernel has it"** - not "the peer received it".
- **The kernel's queues are finite** - full accept queues drop connections; full buffers block
  senders.
- **How a server waits** (one thread per connection vs one event loop for all) determines how
  far it scales.

## Practice projects

- [`practice/socket-management/socket-buffers`](../../practice/socket-management/socket-buffers) -
  send/receive buffers, byte-stream framing, the accept queue.
- [`practice/socket-management/tcp-server-patterns`](../../practice/socket-management/tcp-server-patterns) -
  iterative vs thread-per-connection vs epoll, under slow clients.

## Related

- [DMA](<../memory management/dma.md>) - how packets get between the NIC and RAM.
- [Process management](<../process management/process-management.md>) - the threads and
  processes servers are built from.
- [On timeouts](<../os concepts/on-timeouts.md>) - what happens when the other side stops answering.
