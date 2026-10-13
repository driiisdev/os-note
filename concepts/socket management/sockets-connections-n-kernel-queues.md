# Sockets, Connections and Kernel Queues

A **socket** is an endpoint for network communication: a kernel object that the application
refers to by a file descriptor. A server uses one socket to **listen** and gets a new socket for
**every connection** it accepts. Between those two, the kernel keeps queues the application
never touches directly - and their sizes decide how a server behaves under load.

## The server-side lifecycle

```c
int lfd = socket(AF_INET, SOCK_STREAM, 0);       // 1. create a TCP socket
bind(lfd, &addr, sizeof addr);                   // 2. attach it to an IP:port
listen(lfd, 128);                                // 3. start accepting; 128 = backlog
for (;;) {
    int cfd = accept(lfd, NULL, NULL);           // 4. take one finished connection
    /* read(cfd, ...), write(cfd, ...) */        // 5. talk on the NEW socket
    close(cfd);                                  // 6. FIN, and eventually free it
}
```

- The **listening socket** (`lfd`) never carries data. It only produces new connections.
- Each **connection socket** (`cfd`) is identified by the 4-tuple **(local IP, local port,
  remote IP, remote port)** - which is why one server port can hold thousands of connections.
- The client side is just `socket()` + `connect()`; the kernel picks an ephemeral source port.

## Two queues per listening socket (Linux)

```text
 client                     server kernel                              server app
   SYN  ------------------>  [ SYN queue ]   half-open: SYN received,
                              (SYN_RECV)     SYN-ACK sent, waiting for ACK
   <---------------- SYN-ACK
   ACK  ------------------>  [ accept queue ] fully established,        accept() takes
                              (ESTABLISHED)   waiting for the app  ---->  one from here
```

| Queue | Holds | Size limited by |
| --- | --- | --- |
| **SYN queue** | half-open connections mid-handshake | `net.ipv4.tcp_max_syn_backlog` (and SYN cookies when it overflows) |
| **accept queue** | completed connections the app hasn't `accept()`ed yet | `min(backlog passed to listen(), net.core.somaxconn)` |

**The handshake completes in the kernel**, whether or not the application is ready. The client's
`connect()` returns success as soon as it's in the accept queue - even if the server app is busy,
stuck, or never calls `accept()`.

**When the accept queue is full**, Linux by default **drops** the client's final ACK (or the SYN),
so the client's kernel retransmits after 1s, 3s, 7s... The client sees a slow or hung connect,
not an error. (`net.ipv4.tcp_abort_on_overflow=1` sends a RST instead.) The kernel counts these
drops: `nstat -az | grep -i ListenOverflows`.

🖼️ **Image to add - SYN and accept queues:** a client on the left sending SYN / ACK, the server
kernel in the middle with two queues (SYN queue holding half-open entries, accept queue holding
established ones), and the app on the right pulling from the accept queue with `accept()`; a
"queue full → ACK dropped → client retries" branch.
![SYN and accept queues](<../../assets/img/syn-accept-queues.png>)

## Per-connection kernel state

Each connection socket has:

- a **receive buffer** - data that arrived and was ACKed, waiting for `recv()`;
- a **send buffer** - data the app wrote that hasn't been ACKed by the peer yet;
- the **TCP state** (`ESTABLISHED`, `FIN_WAIT_1`, `CLOSE_WAIT`, `TIME_WAIT`...), sequence
  numbers, window sizes, congestion state, RTT estimates, and timers (retransmission, keepalive,
  delayed ACK).

The application only ever sees "connected", "data available" or "error" - the kernel runs the
whole state machine (see [sending and receiving data](sending-n-receiving-data.md)).

## Seeing it: `ss`

```bash
ss -ltn                  # listening TCP sockets
#  State   Recv-Q  Send-Q   Local Address:Port
#  LISTEN  3       2        127.0.0.1:9100       <- for LISTEN: Recv-Q = accept queue length now,
#                                                   Send-Q = its limit (the backlog)
ss -tn                   # established connections
#  ESTAB   0       2486784  127.0.0.1:43122  127.0.0.1:9100   <- for ESTAB: Recv-Q = unread bytes,
#                                                                Send-Q = unACKed bytes
ss -tni                  # + TCP internals: rtt, cwnd, retransmits
ss -s                    # summary by state
```

## Limits worth knowing

- **File descriptors**: every socket is an fd; `ulimit -n` (often 1024 by default for shells)
  caps them per process. Busy servers raise it to hundreds of thousands.
- **Ephemeral ports**: a *client* talking to one destination IP:port can only have as many
  connections as local ports (`net.ipv4.ip_local_port_range`, ~28K by default). Connections in
  `TIME_WAIT` (kept ~60s after closing) still hold their port.
- **Memory**: socket buffers come out of kernel memory, capped globally by `net.ipv4.tcp_mem`.

## Clarifications

- **`connect()` succeeding ≠ the server application has your connection.** It only means the
  server's *kernel* completed the handshake.
- **The `backlog` argument is not "max connections".** It only sizes the queue of connections
  waiting to be accepted. Accepted connections aren't counted.
- **A listening socket and an accepted socket share the port** - the 4-tuple, not the port,
  identifies a connection.
- **`SO_REUSEPORT`** lets several sockets (in different threads or processes) listen on the same
  port, each with its own accept queue - the kernel load-balances new connections between them.

## Practice project

[`practice/socket-management/socket-buffers`](../../practice/socket-management/socket-buffers) -
`backlog` calls `listen(fd, 2)` and never accepts: 3 of 8 clients complete the handshake
(backlog + 1), the rest have their SYNs dropped, and `ss` shows `Recv-Q 3  Send-Q 2` for the
listening socket. Then `accept()` drains exactly 3.

## Related

- [Sending and receiving data](sending-n-receiving-data.md) - what happens on an accepted socket.
- [Socket programming patterns](socket-programming-patterns.md) - who calls `accept()`, and how often.
- [On timeouts](<../os concepts/on-timeouts.md>) - what a client does while its SYNs are being dropped.
