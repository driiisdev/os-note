# On Timeouts

Every network call can hang: the server is overloaded, a packet is dropped, a router forgot the
connection, the process on the other side is stuck in an infinite loop. A **timeout** is the
decision of how long to wait before giving up. Without one, "how long?" is answered by kernel
defaults - which are measured in **minutes or hours**, or **forever**.

## The timeouts that matter

| Timeout | Guards against | Kernel default if you set nothing (Linux) |
| --- | --- | --- |
| **Connect timeout** | a host that doesn't answer the SYN (down, firewalled, wrong IP) | `tcp_syn_retries = 6` → gives up after **~127 s** (1+2+4+8+16+32+64) |
| **Read timeout** | a server that accepted the request but never replies | **forever** - a blocking `recv()` waits indefinitely |
| **Write / unacknowledged-data timeout** | a peer that vanished while you're sending | `tcp_retries2 = 15` → **~15 minutes** of retransmissions |
| **Idle / keepalive** | a connection silently dropped by a middlebox while idle | keepalive **off**; if on, first probe after **2 hours** (`tcp_keepalive_time`) |
| **Total (deadline)** | everything above combined, plus retries, DNS, TLS... | none - only the application can define it |

```bash
sysctl net.ipv4.tcp_syn_retries net.ipv4.tcp_retries2 \
       net.ipv4.tcp_keepalive_time net.ipv4.tcp_keepalive_intvl net.ipv4.tcp_keepalive_probes
```

## How to set each one (sockets)

```c
// connect timeout: non-blocking connect() + poll() with a deadline
fcntl(fd, F_SETFL, O_NONBLOCK);
connect(fd, ...);                                // -1 / EINPROGRESS
poll(&(struct pollfd){ fd, POLLOUT }, 1, 2000);  // 0 = timed out; then check SO_ERROR

// read / write timeouts on a blocking socket
struct timeval tv = { .tv_sec = 3 };
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);   // recv() -> EAGAIN after 3 s
setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

// keepalive for idle connections
setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE,  &on,    sizeof on);
setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof idle);    // seconds before first probe
setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof intvl);
setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof cnt);

// cap how long sent data may stay unacknowledged before the connection is dropped
setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &ms, sizeof ms);
```

With an event loop, timeouts are timers in the loop (`epoll_wait`'s timeout argument, a timer
wheel or heap) rather than socket options.

🖼️ **where each timeout applies:**
![timeout map](<../../assets/img/timeouts-map.png>)

## Why missing timeouts take systems down

A timeout isn't only about one slow request. A request without a timeout **holds resources** while
it waits: a thread, a pooled connection, memory, a file descriptor.

```text
 downstream service slows down
   -> calls without timeouts pile up, each holding a worker thread
   -> the thread pool is exhausted
   -> every request to THIS service now waits, including ones that never touch the slow dependency
   -> its callers' threads pile up too ...                            (cascading failure)
```

The fix is to make failure **fast and bounded**: a short timeout turns "hang" into an error the
caller can handle - retry, fall back, return a degraded answer, or fail the request cleanly.

## Choosing values

- **Base timeouts on measured latency**, e.g. a little above the p99.9 of normal responses - not
  on round numbers.
- **Connect timeouts can be short** (hundreds of ms inside a datacenter): a healthy server answers
  a SYN in under a millisecond.
- **Deadlines should shrink as they propagate.** If the caller gave you 2s, and you've used 500ms,
  your downstream call gets at most 1.5s - gRPC propagates deadlines this way automatically.
- **Retries need timeouts, back-off and jitter** - immediate retries of timed-out requests multiply
  load on a service that is already struggling.
- **Keepalive and idle limits must be shorter than every middlebox's idle timeout** on the path
  (NAT gateways, firewalls, load balancers): see the
  [Postgres/Cisco incident](postgres-failure-caused-by-a-cisco-router-tcp-issue.md).

## Clarifications

- **A timeout doesn't cancel the work on the other side.** The server may still finish the request
  after the client has given up - which is why retried operations need to be idempotent.
- **Read timeouts are per call, not per request.** A server that sends one byte every 2.9s never
  trips a 3s read timeout - a total deadline catches that.
- **Keepalive ≠ read timeout.** Keepalive detects a *dead path* on an idle connection; it won't
  notice a live server that simply never answers - the kernel on the other side still ACKs the
  probes.

## Practice project

[`practice/os-concepts/timeouts-lab`](../../practice/os-concepts/timeouts-lab) - a misbehaving
server (`hang`, `silent`) and a client where each timeout is opt-in: a connect to an unroutable
address giving up after exactly 2s instead of ~127s, a read timeout turning an endless wait into an
error after 3s, keepalive detecting a silently dropped idle connection, and `make defaults` printing
the kernel values above.

## Related

- [Postgres failure caused by a Cisco router](postgres-failure-caused-by-a-cisco-router-tcp-issue.md) -
  what a missing keepalive looks like in production.
- [Sockets, connections and kernel queues](<../socket management/sockets-connections-n-kernel-queues.md>) -
  dropped SYNs from a full accept queue look exactly like a connect timeout.
- [Sending and receiving data](<../socket management/sending-n-receiving-data.md>) - retransmission
  and ACKs, the machinery behind `tcp_retries2`.
