# Postgres Failure Caused by a Cisco Router (TCP Issue)

A Postgres client kept losing its connection to the database - always after about **90 minutes**
of the connection sitting idle. Both the client and the database server ran Windows. Postgres was
healthy, the client code was fine, and no one had closed anything. The culprit was a **Cisco
router in between**, whose own TCP idle timeout (by default **1 hour**) silently threw away its
record of the connection. Neither end found out until they tried to use it again.

*Scope note: the incident summary is from Hussein Nasser's own post about debugging it (source at
the end), which pointed to Cisco's documentation for the router timeout. The mechanism below is
the standard behavior of stateful middleboxes and TCP keepalive. The general version of this story
is also written up in the backend-engineering notes (`be-note`); this note focuses on the OS and
TCP-stack side.*

## The setup

```text
 [ app + Postgres client ] ---- [ Cisco router / firewall ] ---- [ Postgres server ]
        Windows                    keeps a STATE TABLE of            Windows
                                   every TCP connection through it,
                                   with an idle timer per entry
```

- The client opens a TCP connection and keeps it open - normal for a connection pool, or for a
  long-running tool that only queries occasionally.
- The router tracks every connection passing through it (for NAT, firewall rules, or inspection),
  and **evicts entries that have been idle longer than its timeout** - 1 hour by default here
  (configurable - e.g. `timeout conn` on Cisco ASA firewalls, whose default is also 1 hour).

## What happens, minute by minute

```text
 t = 0        connection established. client, router and server all agree it exists.
 t = 0-60m    no packets - the app has nothing to ask. Both OS kernels still show ESTABLISHED.
 t = 60m      router: "idle for an hour" -> deletes its state entry. Sends nothing to anyone.
 t = 90m      client sends a query.
              router: packet for a connection it doesn't know -> drops it (or rejects it).
              client kernel: no ACK -> retransmits ... and eventually the client errors out.
              server: never saw a thing; its socket still shows ESTABLISHED.
```

🖼️ **silent drop timeline:**
![silent connection drop](<../../assets/img/router-idle-drop.png>)

Why it's so confusing to debug:

- **Nothing fails at the moment of failure.** No RST, no FIN, no log line - TCP itself has no
  idea anything happened, because no TCP endpoint was involved.
- **The symptom shows up later, somewhere else** - as a hang or a "connection lost" in the client,
  which looks like a database or network outage.
- **It depends on idle time**, so it never reproduces in a quick test, only in production overnight
  or between batch jobs.
- **The server side leaks too**: its socket sits in `ESTABLISHED` forever (or until its own
  timers notice), holding a backend process.

## Why TCP didn't save them

TCP has a built-in fix for this: **keepalive** - after a connection has been idle for a while, the
kernel sends a tiny probe that the peer's kernel must ACK. But:

| | Default |
| --- | --- |
| keepalive **enabled**? | only if the application sets `SO_KEEPALIVE` on the socket |
| idle time before the first probe (Linux `tcp_keepalive_time`) | **7,200 s = 2 hours** |
| idle time before the first probe (Windows `KeepAliveTime`) | **2 hours** |
| Postgres server `tcp_keepalives_idle` | `0` = use the OS default |

A 2-hour keepalive can never beat a 1-hour router timeout. By the time the first probe goes out,
the router's entry has been gone for an hour.

## The fix

Make something cross the router **more often than its idle timeout**:

- **Postgres server**, `postgresql.conf`:

  ```ini
  tcp_keepalives_idle = 300        # first probe after 5 minutes idle
  tcp_keepalives_interval = 30     # then every 30 s
  tcp_keepalives_count = 3         # dead after 3 unanswered probes
  ```

- **Client side** (libpq connection string): `keepalives=1 keepalives_idle=300
  keepalives_interval=30 keepalives_count=3` - most drivers and poolers expose the same knobs.
- **Pool side**: limit how long a connection may sit idle in a pool (`idle_timeout`, `max_lifetime`)
  so it's recycled before any middlebox forgets it, and validate connections before handing them out.
- **Network side**: raise the router's idle timeout for the database flows (fine as a mitigation,
  but every middlebox you'll ever add needs it).

Keepalive helps in two ways: the probes count as traffic, so the router's idle timer keeps being
reset; and if the path *is* broken, the probes fail fast, the kernel declares the connection dead
(`ETIMEDOUT`), and the application finds out in seconds instead of on its next query.

## The OS lessons

- **"ESTABLISHED" is only each kernel's opinion.** Two endpoints can both believe in a connection
  the network has already forgotten.
- **Kernel defaults are tuned for the 1980s internet** - 2-hour keepalives, 15-minute
  retransmission timeouts - and are almost never right for a datacenter. Long-lived connections
  need explicit timeouts and keepalives (see [on timeouts](on-timeouts.md)).
- **Middleboxes are part of your system** - NAT gateways, firewalls, load balancers and routers
  all keep per-connection state with their own timeouts (AWS NAT gateway: 350 s idle).

## Practice project

[`practice/os-concepts/timeouts-lab`](../../practice/os-concepts/timeouts-lab) - scenario 3
reproduces this: a server that holds an idle connection, an `iptables` rule standing in for the
router that silently drops the flow, and a client that hangs forever without keepalive but gets
`ETIMEDOUT` after ~11 seconds with `--keepalive 5,2,3`.

## Sources

- Hussein Nasser's post on debugging the issue (mirror): [rattibha.com thread](https://en.rattibha.com/thread/1861570729600483794)

## Related

- [On timeouts](on-timeouts.md) - every TCP timer that decides how long "hung" lasts.
- [Sockets, connections and kernel queues](<../socket management/sockets-connections-n-kernel-queues.md>) -
  the per-connection state each kernel keeps.
- [Socket programming patterns](<../socket management/socket-programming-patterns.md>) - why
  Postgres's process-per-connection model makes leaked connections expensive.
