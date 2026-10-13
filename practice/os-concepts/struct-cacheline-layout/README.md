# Practice: Struct Cache-Line Layout

Concept note: [`how-google-improved-linux-tcp-ip-stack-by-40-percent.md`](<../../../concepts/os concepts/how-google-improved-linux-tcp-ip-stack-by-40-percent.md>)

## What this demonstrates

The two cache-line effects behind the Linux networking-struct reorganization the concept note
covers, in miniature:

1. **`layout`** - the same struct, the same fields, the same loop. Only the **order** of the
   fields changes: hot fields scattered over 4 cache lines vs grouped into 1.
2. **`falseshare`** - two threads, two *different* counters, no shared data - but if both
   counters sit in one 64-byte cache line, every write forces that line to bounce between
   cores. **False sharing.**

## Folder structure

```text
struct-cacheline-layout/
  layout.c       <- scattered vs grouped hot fields, random access, several working-set sizes
  falseshare.c   <- adjacent vs 64-byte-separated counters, threads pinned to different cores
  Makefile       <- `make`, `make demo`, `make pahole` (needs: sudo apt install dwarves)
```

## Build & run

```bash
make demo
make pahole       # print each struct's layout with cache-line boundaries marked
```

## What you'll actually see

```text
sizeof: scattered 256 B, grouped 256 B (same size, same fields)
hot field offsets  scattered: 0 64 128 192   grouped: 0 8 16 24

structs        hot data      hot data     scattered       grouped  speedup
            (scattered)     (grouped)            ms            ms
1024             256 KB         64 KB         193.8         184.8     1.0x
4096            1024 KB        256 KB         223.5         199.8     1.1x
16384           4096 KB       1024 KB         769.6         675.9     1.1x
65536          16384 KB       4096 KB        1505.7        1152.7     1.3x
262144         65536 KB      16384 KB        1501.0        1376.7     1.1x

2 threads (CPU 0 and CPU 2, different cores) x 100000000 increments of their OWN counter
  counters in the same cache line:   333.8 ms
  counters 64 bytes apart        :   191.2 ms   -> 1.7x faster
```

**Read this result honestly - it's smaller than "4 lines vs 1 line = 4x"**, and *why* is the
interesting part:

- When everything fits in L1/L2 (first rows), layout barely matters: hits are cheap either way.
- The biggest gain (**1.3x**) is at 65,536 structs, where the **grouped hot data (4MB) still fits
  in the 6MB L3 but the scattered hot data (16MB) doesn't**. Fewer cache lines → the working set
  fits in a faster level of cache. That's the real mechanism.
- It isn't 4x because the 4 loads per struct are **independent**, so an out-of-order core
  issues them in parallel (memory-level parallelism), and Intel's spatial prefetcher fetches
  cache lines in pairs. The hardware already hides part of a bad layout.
- That magnitude matches reality: the kernel patches are reported at **up to ~40%** on specific
  many-connection workloads - a big win for a kernel, but not 4x.
- **False sharing (1.7x)** is the more dramatic effect: each increment needs the line in
  exclusive (MESI "Modified") state, so it ping-pongs between the cores' caches.

## Code walkthrough

```c
struct scattered {                          // hot fields at offsets 0, 64, 128, 192
    long rx_bytes;      char stats_a[56];   // line 0
    long rx_packets;    char config_b[56];  // line 1
    long state;         char debug_c[56];   // line 2
    long window;        char misc_d[56];    // line 3
};
struct grouped {                            // hot fields at offsets 0, 8, 16, 24
    long rx_bytes, rx_packets, state, window;   // line 0: everything the hot loop reads
    char stats_a[56], config_b[56], debug_c[56], misc_d[56];
};
```

Visits are in **random order** (like packets arriving for random sockets). A sequential sweep
would let the prefetcher stream every line in, hiding the difference almost completely.

```c
struct same_line { volatile long a; volatile long b; };                 // 8 bytes apart
struct own_lines { volatile long a; char pad[56]; volatile long b; };   // 64 bytes apart
```

The threads are pinned to different **physical** cores (`core_id` from sysfs). Hyperthread
siblings share an L1 cache, so the line would have nowhere to bounce.

## Try this yourself

- Pin both `falseshare` threads to hyperthread siblings (CPU 0 and 1 here; check
  `cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list`). The gap should mostly vanish.
- Replace `char pad[56]` with C11's `_Alignas(64)` on `b` - the idiomatic way to do it.
- `make pahole` - `pahole` prints every field's offset and size and marks each cache-line
  boundary. It's the tool used to review the real kernel patches.
- Make `layout.c` *write* one hot field as well as reading four - then run two threads over
  the same array.

## Real-world equivalent

- The Linux 6.8 networking patches reorganized `struct tcp_sock`, `struct net_device`,
  `struct netns_ipv4` and friends into explicitly grouped "read-mostly", "read-write" and
  "cold" sections, with build-time assertions to keep hot fields in place as the structs evolve.
- Java's `@Contended`, Rust's `crossbeam::CachePadded`, C++17's
  `std::hardware_destructive_interference_size`, and the kernel's `____cacheline_aligned` all
  exist to prevent false sharing.
- LMAX Disruptor (a famously fast Java queue) pads its sequence counters for exactly this reason.
