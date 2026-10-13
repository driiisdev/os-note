# Practice: Cache Locality

Concept notes:
[`the-anatomy-of-memory.md`](<../../../concepts/memory management/the-anatomy-of-memory.md>) ·
[`reading-n-writing-from-n-to-memory.md`](<../../../concepts/memory management/reading-n-writing-from-n-to-memory.md>) ·
[`memory-section-demo.md`](<../../../concepts/memory management/memory-section-demo.md>)

## What this demonstrates

That the CPU never reads "one int" from RAM - it reads a whole **64-byte cache line**, and what
you do with the other 60 bytes decides how fast your code is.

1. **Traversal order** - summing the same 64MB grid row by row vs column by column. Same data,
   same number of additions, ~20x difference.
2. **Stride sweep** - touching every 1st, 2nd, 4th ... 256th int of a big array. Up to stride 16
   (= one int per cache line) the *total* time barely changes, even though the work shrinks
   16x - because every cache line still has to be fetched.

## Folder structure

```text
cache-locality/
  locality.c   <- both experiments
  Makefile     <- `make`, `make perf` (cache-miss counters; needs perf, not in stock WSL)
```

## Build & run

```bash
make
./bin/locality
```

## What you'll actually see

```text
1) traversal order over a 4096 x 4096 int grid (64 MB)
   row-major   :     9.3 ms
   column-major:   180.2 ms   (19.4x slower, same 16777216-element sum)

2) stride sweep over a 64MB array (a cache line holds 16 ints)
   stride     accesses     total ms    ns / access
   1          16777216         12.8           0.76
   2           8388608         12.4           1.48
   4           4194304         11.2           2.67
   8           2097152         11.8           5.63
   16          1048576         11.6          11.08
   32           524288          8.0          15.35
   64           262144          5.0          19.01
   128          131072          2.3          17.60
   256           65536          1.5          22.97
```

Look at the **total ms** column: strides 1 to 16 all cost ~12ms. Doing 1/16th of the work
saved nothing, because the bottleneck was never the arithmetic - it was moving 64MB of cache
lines from RAM. Only past stride 16 do whole lines get skipped, and the time finally drops.

## Code walkthrough

```c
static int grid[4096][4096];   // C arrays are row-major: grid[i][0..4095] are adjacent

for (int i = 0; i < N; i++)        // row-major: consecutive addresses,
    for (int j = 0; j < N; j++)    // 16 useful ints per 64-byte line fetched
        sum += grid[i][j];

for (int j = 0; j < N; j++)        // column-major: each access is 16KB (one row) further on,
    for (int i = 0; i < N; i++)    // so every single access is a new cache line - and with
        sum += grid[i][j];         // 4096 rows in flight, lines are evicted before reuse
```

Column-major also defeats the **hardware prefetcher** less obviously than you'd think - modern
prefetchers detect constant strides - but it still wastes 60 of every 64 bytes fetched, and
it also touches a new 4KB *page* on every access, which stresses the TLB too.

The Makefile compiles with `-fno-tree-vectorize`. Without it, gcc turns the row-major loop
into SIMD instructions (8 ints per instruction) and the gap gets even bigger - but then you're
measuring two effects at once.

## Try this yourself

- Shrink `N` to 256 (a 256KB grid that fits in L2) - the gap nearly vanishes, because once the
  whole grid is cached, access order stops mattering.
- Change `int` to `char` in part 2: a cache line now holds 64 elements, so the flat region of
  the total-time column should extend to stride 64.
- Remove `-fno-tree-vectorize` from the Makefile and rerun.

## Real-world equivalent

This is why **column-oriented databases** (ClickHouse, DuckDB, Parquet files) are fast for
analytics: `SUM(price)` reads one tightly packed array instead of skipping across whole rows.
It's why game engines use "struct of arrays" instead of "array of structs", why matrix
libraries tile their loops, and why NumPy cares whether an array is C-order or Fortran-order.
Same story, one level down, in the [Google TCP note](<../../../concepts/os concepts/how-google-improved-linux-tcp-ip-stack-by-40-percent.md>).
