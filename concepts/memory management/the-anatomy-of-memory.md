# The Anatomy of Memory

"Memory" usually means **RAM** - fast, byte-addressable, and **volatile** (it forgets everything
when power is lost). But physically it's a hierarchy of very different technologies, and main
memory itself is organized in a way that makes some access patterns much cheaper than others.

## Two kinds of RAM

| | **SRAM** (static RAM) | **DRAM** (dynamic RAM) |
| --- | --- | --- |
| Cell | ~6 transistors (a flip-flop) | 1 transistor + 1 capacitor |
| Speed | very fast (~1ns) | slower (~50-100ns for a random access) |
| Density / cost | low density, expensive | high density, cheap |
| Needs refresh? | no - holds its value while powered | **yes** - capacitors leak; every row is refreshed every ~64ms |
| Used for | CPU registers' neighbours: **L1/L2/L3 caches** | **main memory** (your 8/16/32GB) |

That trade-off is the reason the memory hierarchy exists: you can have a little memory that's
fast or a lot that's cheap, but not both.

```text
         faster, smaller, more expensive per byte
               ^
   registers   |   < 1 KB per core          ~0.3 ns
   L1 cache    |   32-64 KB per core        ~1 ns      SRAM, inside each core
   L2 cache    |   256 KB - 2 MB per core   ~4 ns      SRAM, inside each core
   L3 cache    |   several MB - 100s of MB  ~10-40 ns  SRAM, shared by all cores
   DRAM        |   GBs                      ~80-150 ns DRAM, on DIMMs
   SSD         |   100s of GB - TBs         ~10-100 us flash (persistent)
   HDD         v   TBs                      ~5-10 ms   magnetic (persistent)
```

🖼️ **memory hierarchy pyramid:**
![memory hierarchy](<../../assets/img/memory-hierarchy.png>)

## How DRAM is organized

Main memory is not one flat array of bytes. From the outside in:

```text
 memory controller (inside the CPU)
   └─ channel  (independent 64-bit bus; 2 channels = 2x bandwidth)
       └─ DIMM  (the stick you plug in)
           └─ rank (a set of chips that respond together)
               └─ chip
                   └─ bank (independent array; 16-32 per chip on DDR4/5)
                       └─ rows x columns of cells
                              ^
                              a row ("page", typically 1-8KB) is opened into
                              the bank's ROW BUFFER before any column can be read
```

Reading DRAM involves:

1. **Activate** - copy a whole row from the cell array into the bank's **row buffer**
   (the capacitors are drained by reading; the row buffer holds the data and later writes it back).
2. **Read/write columns** - transfer data from the row buffer in **bursts** (DDR4/DDR5 move
   64 bytes per burst - exactly one CPU cache line).
3. **Precharge** - close the row so a different one can be opened.

Consequences:

- **Reading from an already-open row is much faster** than opening a new one ("row buffer hit"
  vs "row miss"). Sequential access mostly hits the same open rows; random access keeps
  opening new ones.
- **The CPU always fetches whole 64-byte cache lines**, never single bytes.
- **Bandwidth comes from parallelism** - multiple channels, ranks and banks working at once.
  The memory controller interleaves consecutive cache lines across them.

**DDR** = *Double Data Rate*: data is transferred on both the rising and falling edge of the
clock. DDR5-4800 moves 4,800 million transfers per second per channel.

## Other properties worth knowing

- **Volatility** - DRAM loses its contents within a fraction of a second without power and
  refresh. Everything in RAM is lost on a crash or power cut unless it was written to storage -
  the reason [`fsync`](<../storage management/what-really-happens-in-a-file-io.md>) exists.
- **ECC memory** - server DIMMs store extra bits per word to detect and correct single-bit
  errors (cosmic rays, failing cells). Consumer RAM usually doesn't.
- **NUMA** - on multi-socket servers, each CPU socket has its own memory controller and local
  DIMMs. Accessing the *other* socket's memory is slower, so the OS tries to allocate memory on
  the node where the thread runs (`numactl --hardware` shows the layout).
- **Rowhammer** - repeatedly activating one DRAM row can flip bits in neighbouring rows, a
  hardware flaw that has been turned into real security exploits.

## Clarifications

- **"Memory" vs "storage".** Memory (RAM) is byte-addressable by CPU instructions and volatile.
  Storage (SSD/HDD) is accessed in blocks through a driver and is persistent. (Persistent memory
  like the discontinued Intel Optane blurred this line.)
- **A DRAM "page" (row) is not an OS "page".** Same word, different things: a DRAM row is a
  hardware unit inside a chip; an OS page is the 4KB unit of virtual memory.
- **Latency vs bandwidth.** A single random access takes ~100ns (latency), but the memory
  system can deliver tens of GB/s (bandwidth) when many accesses are in flight at once.
  Sequential code benefits from bandwidth; pointer-chasing code is stuck with latency.

## Practice project

[`practice/memory-management/cache-locality`](../../practice/memory-management/cache-locality) -
the stride sweep shows the 64-byte cache line directly: touching every 16th int costs the same
as touching every int, because each access still pulls in a whole line. The latency ladder in
[`practice/inside-the-cpu/cpu-wait-times`](../../practice/inside-the-cpu/cpu-wait-times)
measures every level of the hierarchy table above.

## Related

- [Reading and writing from and to memory](reading-n-writing-from-n-to-memory.md) - the path a
  single load or store takes through this hierarchy.
- [CPU components and architecture](<../inside the cpu/cpu-components-n-architecture.md>) - where
  the caches and the memory controller sit.
