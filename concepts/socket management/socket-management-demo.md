# Socket Management Demo

The lecture demo shows sockets as the kernel sees them: listening sockets and their queues,
established connections with data waiting in buffers, and how the server's design shows up in
those numbers. Below is a reproducible version; the lecture's exact steps can be added from the
`.txt`.

## 1. A server you can poke

```bash
python3 -m http.server 8000 &          # any server works
ss -ltnp 'sport = :8000'               # LISTEN, Recv-Q 0, Send-Q = backlog, owning process
curl -s localhost:8000 > /dev/null
ss -tan 'sport = :8000 or dport = :8000'   # the closed connection lingers in TIME-WAIT
```

## 2. Reading `ss`

| Socket state | `Recv-Q` | `Send-Q` |
| --- | --- | --- |
| `LISTEN` | connections currently in the **accept queue** | the accept queue's **limit** (backlog) |
| `ESTAB` and others | bytes received but **not yet read** by the app | bytes sent but **not yet ACKed** by the peer |

- A listening socket with `Recv-Q` near `Send-Q` → the app isn't calling `accept()` fast enough.
- An established socket with a growing `Recv-Q` → the app isn't reading.
- A growing `Send-Q` → the peer or the network is slow.

```bash
ss -tni                     # per-connection TCP internals: rtt, cwnd, retrans, send rate
ss -s                       # totals by state - a lot of TIME-WAIT is normal on busy clients
nstat -az | grep -iE 'ListenOverflows|ListenDrops|Retrans'   # kernel counters
ls -l /proc/<pid>/fd | grep socket     # sockets are file descriptors
```

## 3. Fill an accept queue

```bash
cd practice/socket-management/socket-buffers && make && ./bin/backlog
```

```text
client 1: connected      (handshake done, sitting in the accept queue)
client 2: connected      (handshake done, sitting in the accept queue)
client 3: connected      (handshake done, sitting in the accept queue)
client 4: still waiting  (SYN dropped - accept queue full, kernel will retry)
...
State  Recv-Q Send-Q Local Address:Port
LISTEN 3      2          127.0.0.1:9100
```

## 4. Fill a send buffer

```bash
./bin/buffers
```

```text
   send() accepted 2493 KB before returning EAGAIN - and the server read NOTHING
     client send queue   (SIOCOUTQ):   2429 KB  - sent, waiting for the peer
     server receive queue (SIOCINQ):     63 KB  - arrived, waiting for recv()
```

Run `ss -tn` from another terminal while it's paused (add a `sleep` before the `recv`) to see the
same numbers in `Send-Q` and `Recv-Q`.

## 5. Watch a server pattern under load

```bash
cd ../tcp-server-patterns && make demo
```

```text
  => 10 clients x 500ms think time, all served in 5006 ms     <- iterative
  => 10 clients x 500ms think time, all served in 502 ms      <- threaded
  => 10 clients x 500ms think time, all served in 501 ms      <- epoll
```

While the iterative server runs, `ss -ltn 'sport = :9201'` shows clients piling up in its accept
queue; `ls /proc/$(pgrep threaded)/task | wc -l` shows one thread per active client.

🖼️ **Image to add - ss output annotated:**
![ss output annotated](<../../assets/img/ss-annotated.png>)

## What to take away

- **`ss` is the window into the kernel's socket queues** - learn to read `Recv-Q` and `Send-Q`.
- **Handshakes and buffering happen without the app.** An overloaded app shows up as full queues,
  and its clients see timeouts rather than errors.
- **Server architecture is visible from outside**: thread counts, accept-queue depth and latency
  under slow clients tell you which pattern a server uses.

## Practice projects

- [`practice/socket-management/socket-buffers`](../../practice/socket-management/socket-buffers)
- [`practice/socket-management/tcp-server-patterns`](../../practice/socket-management/tcp-server-patterns)

## Related

- [Sockets, connections and kernel queues](sockets-connections-n-kernel-queues.md)
- [Sending and receiving data](sending-n-receiving-data.md)
- [Socket programming patterns](socket-programming-patterns.md)
