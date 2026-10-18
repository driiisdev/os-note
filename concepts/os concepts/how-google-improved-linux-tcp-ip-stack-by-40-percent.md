# How Google Improved the Linux TCP/IP Stack by 40%

Linux 6.8 (early 2024) shipped a networking change from Google engineers that, in Linus Torvalds'
merge summary, **"improve[s] TCP performances with many concurrent connections up to 40%"**. It
added no new algorithm and no new feature. It **moved fields around inside a few kernel structs**,
so that the code handling every packet touches fewer **cache lines**.

*Scope note: facts below are from the patch series and news coverage (sources at the end). "40%"
is an "up to" number from the best cases - mostly on AMD EPYC servers with many concurrent flows -
not a typical result.*

## The problem: structs that grew for decades

Core networking structs like `struct tcp_sock` (one per TCP connection), `struct net_device` (one
per network interface) and `struct netns_ipv4` (per-namespace IPv4 settings) are large - hundreds
of fields, several KB - and have grown by accretion over 30 years. Fields were added wherever was
convenient, so the handful of fields that the **fast path** (sending and receiving a packet) reads
on *every* packet were scattered across many 64-byte cache lines, mixed in with fields that are
almost never touched.

Recall from [reading and writing memory](<../memory management/reading-n-writing-from-n-to-memory.md>):
the CPU always fetches whole 64-byte lines. If the 10 fields you need live in 10 different lines,
you pay for 10 line fetches - and those 10 lines occupy cache space that could have held *other
connections'* hot data.

```text
 before: hot fields (H) scattered through the struct
 line 0 [H . . . . . . .]   line 1 [. . . H . . . .]   line 2 [. . . . . . . .]
 line 3 [. H . . . . . .]   line 4 [. . . . . . H .]   line 5 [. . . . . . . .] ...
 -> the fast path touches lines 0, 1, 3, 4, ... for every packet

 after: hot fields grouped by how they're used
 line 0 [H H H H H H H H]   <- read-mostly fast-path fields (TX / RX groups)
 line 1 [H H H H . . . .]   <- read-write fast-path fields, kept apart from read-mostly ones
 line 2+ [cold fields ...] <- configuration, statistics, rarely-used state
 -> the fast path touches 2 lines
```

🖼️ **struct reorganization:**
![struct cacheline reorganization](<../../assets/img/struct-cacheline-reorg.png>)

## What the patches did

The series - *"Analyze and Reorganize core Networking Structs to optimize cacheline consumption"*
by Coco Li (Google), merged for Linux 6.8:

1. **Analyzed which fields the TCP/IP fast paths actually access**, and how (read-only vs written).
2. **Reordered the fields** of the core structs into explicit **cache-line groups**: the fields
   used together on the transmit path, on the receive path, read-mostly vs read-write.
3. **Separated read-mostly from read-write fields**, so that writes to one (e.g. counters and
   sequence numbers updated per packet) don't keep invalidating the cache line that other cores
   are reading - avoiding the cache-line bouncing behind
   [false sharing](<../memory management/reading-n-writing-from-n-to-memory.md>).
4. **Added build-time checks** so that a future patch adding a field in the wrong place gets
   caught, instead of the layout slowly decaying again.

Reported results: gains of up to ~40% (individual tests up to ~44%) in TCP throughput with many
concurrent connections, verified with IPv4 and IPv6 on AMD EPYC and IPv6 on Intel Xeon, with the
largest gains on AMD.

## Why "many concurrent connections" is the key phrase

With one connection, its `tcp_sock` stays hot in L1/L2 no matter how badly it's laid out. With
thousands of active connections, each packet hits a different connection's struct, and the
**total hot data across all connections** competes for cache. Fewer lines per connection means:

- fewer cache misses per packet, and
- more connections' hot data fits in L2/L3 at once.

The win comes from **capacity**: does the working set fit in a faster level of cache or not? It
doesn't come from making any single access faster. That's also why the gain varies by CPU:
different cache sizes and hierarchies (AMD EPYC's L3 is split into per-chiplet slices) put the
"fits / doesn't fit" boundary in different places.

## The OS lessons

- **Data layout is performance.** Same code, same algorithm, a significant change in throughput.
- **At scale, the kernel's own data structures are a cache-pressure problem**, just like an
  application's.
- **Hot/cold splitting** and **read/write separation** are general techniques: databases, game
  engines and JVMs use them too.
- **Guard rails matter.** Without the build-time assertions, the next 30 years of patches would
  scatter the fields again.

## Practice project

[`practice/os-concepts/struct-cacheline-layout`](../../practice/os-concepts/struct-cacheline-layout) -
the same 256-byte struct with hot fields scattered over 4 cache lines vs grouped into 1, visited in
random order over growing working sets (up to 1.3x faster where the grouped hot data fits in L3 and
the scattered one doesn't - with an honest explanation of why modern cores hide much of the
difference), plus a false-sharing benchmark (1.7x).

## Sources

- [Phoronix: Linux 6.8 Network Optimizations Can Boost TCP Performance For Many Concurrent Connections By ~40%](https://phoronix.com/news/Linux-6.8-Networking)
- [Gigazine: Linux kernel 6.8 is expected to optimize the network and improve TCP concurrent connection performance by about 40%](https://gigazine.net/gsc_news/en/20240111-linux-6-8-networking)
- [LWN discussion of the patch series](https://lwn.net/Articles/951321)

## Related

- [The anatomy of memory](<../memory management/the-anatomy-of-memory.md>) - why lines, not bytes.
- [Sending and receiving data](<../socket management/sending-n-receiving-data.md>) - the fast path
  these structs sit on.
- [CPU wait times demo](<../inside the cpu/inside-the-cpu-demo-cpu-wait-times.md>) - the cost of
  each cache miss.
