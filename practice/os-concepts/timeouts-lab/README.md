# Practice: Timeouts Lab

Concept notes:
[`on-timeouts.md`](<../../../concepts/os concepts/on-timeouts.md>) ·
[`postgres-failure-caused-by-a-cisco-router-tcp-issue.md`](<../../../concepts/os concepts/postgres-failure-caused-by-a-cisco-router-tcp-issue.md>)

## What this demonstrates

What happens when a network call has **no timeout**, and how each kind of timeout turns "hang
forever" into a fast, handleable error. A deliberately misbehaving server plus a client where
every timeout is opt-in:

| Scenario | Without a timeout | With one |
| --- | --- | --- |
| **Connect** to a host that drops SYNs | ~2 minutes (6 SYN retries) | `--connect-timeout` |
| **Read** from a server that never answers | forever | `--read-timeout` |
| **Idle** pooled connection, silently dropped by a middlebox | forever - *the Postgres/Cisco incident* | `--keepalive` |

## Folder structure

```text
timeouts-lab/
  server.c    <- normal | hang (reads, never replies) | silent (never reads or writes)
  client.c    <- --connect-timeout, --read-timeout, --keepalive IDLE,INTVL,COUNT, --idle
  Makefile    <- `make`, `make demo` (scenarios 1 + 2), `make defaults` (kernel sysctls)
```

## Build & run

```bash
make demo        # connect timeout + read timeout, no root needed
make defaults    # the kernel values you get when you set nothing
```

Scenario 3 needs `iptables` to play the part of the broken router (`sudo apt install iptables`).
Run each command in its own terminal:

```bash
# terminal 1: a server that holds connections open and never says anything
./bin/server silent

# terminal 2: a pooled-style client: connect, then sit idle waiting for data
./bin/client --idle --keepalive 5,2,3      # probe after 5s idle, every 2s, dead after 3 misses
#   ...and once more WITHOUT --keepalive, to see the hang

# terminal 3: once both clients say "connected", make the "router" silently drop the flow
sudo iptables -I INPUT -p tcp --dport 9300 -j DROP

# clean up afterwards!
sudo iptables -D INPUT -p tcp --dport 9300 -j DROP
```

## What you'll actually see

```text
$ make demo
== 1) connect timeout: a SYN sent into the void (10.255.255.1 drops it)
  connecting to 10.255.255.1:9300 (with a connect timeout)
  [  2.00s] CONNECT TIMEOUT - giving up on the handshake

== 2) read timeout: the server accepts the request and never answers
server (hang) on :9300
  connecting to 127.0.0.1:9300 (no connect timeout - kernel default)
  [  0.00s] connected
  [  0.00s] sent request, waiting for the reply...
  [  3.06s] READ TIMEOUT - no reply in time, the caller can retry or fail fast

$ make defaults
net.ipv4.tcp_syn_retries = 6           <- connect gives up after ~127s (1+2+4+8+16+32+64)
net.ipv4.tcp_retries2 = 15             <- unacknowledged DATA: ~15 minutes before giving up
net.ipv4.tcp_keepalive_time = 7200     <- keepalive (if enabled at all!) starts after 2 HOURS
net.ipv4.tcp_keepalive_intvl = 75
net.ipv4.tcp_keepalive_probes = 9
```

Scenario 3, expected (the iptables rule needs `sudo`, so it wasn't run while writing these
notes - this is the kernel's documented keepalive behavior, timed from the parameters):

```text
$ ./bin/client --idle --keepalive 5,2,3
  keepalive on: first probe after 5s idle, then every 2s, dead after 3 misses
  connecting to 127.0.0.1:9300 (no connect timeout - kernel default)
  [  0.00s] connected
  [  0.00s] idle - sitting on the connection like a pooled client...
  [ ~11.0s] ETIMEDOUT - keepalive probes went unanswered, connection declared dead

$ ./bin/client --idle
  [  0.00s] connected
  [  0.00s] idle - sitting on the connection like a pooled client...
  (nothing. ever. both ends still show ESTABLISHED in `ss -tn`)
```

## Code walkthrough

**Connect timeout** - a blocking `connect()` can't be given a deadline, so make it
non-blocking and wait for it with `poll()`:

```c
fcntl(fd, F_SETFL, O_NONBLOCK);
connect(fd, ...);                                   // returns -1 / EINPROGRESS immediately
struct pollfd p = { .fd = fd, .events = POLLOUT };
if (poll(&p, 1, connect_ms) == 0) /* timed out */;
getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);   // writable can also mean "failed"
```

**Read timeout** - one socket option, then `recv()` returns `EAGAIN` instead of waiting forever:

```c
struct timeval tv = { read_ms / 1000, (read_ms % 1000) * 1000 };
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
```

**Keepalive** - off by default; and when it's on, the system default waits 2 hours:

```c
setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE,  &on,       sizeof on);
setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &ka_idle,  sizeof ka_idle);   // seconds idle
setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &ka_intvl, sizeof ka_intvl);  // between probes
setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &ka_cnt,   sizeof ka_cnt);    // misses = dead
```

A keepalive probe is an empty TCP segment the peer's kernel must ACK. If the path is broken,
no ACK comes back, and after `COUNT` misses the kernel kills the connection - and a blocked
`recv()` finally returns `ETIMEDOUT`.

## Try this yourself

- Run `./bin/client --host 10.255.255.1` with **no** connect timeout and time it (`time ...`).
  Then `sudo sysctl net.ipv4.tcp_syn_retries=2` and try again.
- Scenario 3 variant: drop the traffic **while the client is sending**, not idle (use
  `./bin/server hang`, start the client without `--read-timeout`, then add the rule). Keepalive
  doesn't help here - unacknowledged data is governed by `tcp_retries2` (~15 min). Fix it with
  `setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &ms, sizeof ms)`.
- Watch the probes: `sudo tcpdump -i lo -n port 9300` during scenario 3.

## Real-world equivalent

- **Postgres**: `tcp_keepalives_idle`, `tcp_keepalives_interval`, `tcp_keepalives_count` (server
  side) and `keepalives_idle=...` in the libpq connection string (client side) - exactly
  `--keepalive`. PgBouncer and most drivers expose the same knobs.
- **AWS NAT gateways and load balancers** drop idle TCP flows after 350s / 60s by default; many
  firewalls after an hour or less. The kernel's 2-hour keepalive default will never save you
  from them.
- Every HTTP client library has (or should have) separate **connect**, **read** and **total**
  timeouts. Missing ones are a top cause of thread-pool exhaustion: every thread ends up stuck
  in a `recv()` that will never return.
