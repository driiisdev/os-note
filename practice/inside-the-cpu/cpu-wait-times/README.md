# Practice: CPU Wait Times

Concept notes:
[`inside-the-cpu-demo-cpu-wait-times.md`](<../../../concepts/inside the cpu/inside-the-cpu-demo-cpu-wait-times.md>) ·
[`pipelining-n-parallelism.md`](<../../../concepts/inside the cpu/pipelining-n-parallelism.md>) ·
[`cpu-components-n-architecture.md`](<../../../concepts/inside the cpu/cpu-components-n-architecture.md>)

## What this demonstrates

Three measurements of the ways a real CPU ends up **waiting instead of computing**:

1. **`latency`** - how long one load takes depending on where the data lives: L1, L2, L3 or
   RAM. Pointer-chasing through a random cycle makes every load depend on the previous one, so
   nothing can be overlapped or prefetched - the time per step *is* the latency.
2. **`branch`** - the same loop over the same numbers, shuffled vs sorted. Identical work; the
   only difference is whether the **branch predictor** can guess the `if`.
3. **`ilp`** - the same additions done with 1 accumulator vs 4. Identical work; the only
   difference is whether the additions **depend on each other** (a data hazard) or can run in
   parallel inside the core's pipelines.

## Folder structure

```text
cpu-wait-times/
  latency.c   <- pointer-chasing latency ladder, 4KB -> 256MB working sets
  branch.c    <- branch prediction: shuffled vs sorted data
  ilp.c       <- instruction-level parallelism: dependent vs independent additions
  Makefile    <- `make`, `make demo`
```

## Build & run

```bash
make demo
lscpu | grep -i cache    # to match the latency jumps against your cache sizes
```

## What you'll actually see

On an Intel i5-8365U (L1d 32KB/core, L2 256KB/core, L3 6MB shared) under WSL2:

```text
working set  ns / load
      4 KB       1.10  ##
     16 KB       1.04  ##
     32 KB       1.22  ##                 <- fits in L1d: ~4 cycles
     64 KB       3.51  #######
    128 KB       4.45  ########
    256 KB       6.09  ############       <- L2: ~12-14 cycles
    512 KB      11.76  #######################
      1 MB      26.69  #####################################################
      2 MB      26.65  #####################################################   <- L3
      4 MB      74.19  ############################################################
      8 MB     105.67  ############################################################
     32 MB     116.97  ############################################################
    256 MB     153.21  ############################################################  <- RAM

shuffled data:   244.0 ms   (sum 6299058000)
sorted data  :    36.1 ms   (sum 6299058000, identical work)
mispredictions made the shuffled run 6.8x slower

1 accumulator :   187.2 ms  (sum 335503360000)
4 accumulators:    56.1 ms  (sum 335503360000)
same number of additions, 3.3x faster when they're independent
```

- **~1ns vs ~150ns**: a load that misses every cache costs the same time as ~400 simple
  instructions at this clock speed. That's the "CPU wait time" - the core is stalled.
- The RAM numbers keep creeping up past 8MB because **TLB misses** pile on top of cache misses:
  random access over 256MB touches far more pages than the TLB can map.
- **6.8x for branch prediction**: with shuffled data the `if` is a coin flip, so ~50% of guesses
  are wrong, and each wrong guess throws away ~15-20 cycles of speculative work in the pipeline.
- **3.3x for ILP**: a floating-point add has ~4 cycles of latency but the core can *start* a
  new one every cycle - as long as it doesn't need the previous result.

## Code walkthrough

**Latency** - Sattolo's shuffle builds one random cycle through every node, one node per
64-byte cache line:

```c
struct node { struct node *next; char pad[56]; };
for (long i = 0; i < steps; i++) p = p->next;   // can't start load N+1 until load N returns
```

**Branch prediction**:

```c
for (int i = 0; i < N; i++)
    if (data[i] >= 128)     // shuffled: unpredictable. sorted: false x16K, then true x16K
        sum += data[i];
```

**ILP**:

```c
one += data[i];             // every add waits for the previous add (a dependency chain)

a += data[i];               // four independent chains - the out-of-order core runs
b += data[i + 1];           // them side by side
c += data[i + 2];
d += data[i + 3];
```

The compiler can't do this rewrite for you, because floating-point addition isn't associative
- `(a+b)+c` can differ from `a+(b+c)` in the last bit. (It will with `-ffast-math`.)

**Compiler flags matter**: the Makefile uses `-fno-tree-vectorize -fno-if-conversion`. Without
them, gcc turns the `if` in `branch.c` into a branchless `cmov` (no branch → nothing to
mispredict) and vectorizes `ilp.c` with SIMD - which are the *fixes* for these problems, so
the problems disappear. Try `make clean && make CFLAGS=-O2 demo` to watch the compiler do it.

## Try this yourself

- In `latency.c`, change the loop to step by `kb += kb / 2` instead of doubling, for a finer
  ladder - then find exactly where each jump happens and compare with `lscpu`'s L1d/L2/L3 sizes
  (divide the "N instances" totals by your core count).
- In `branch.c`, replace the `if` with a branchless version:
  `sum += data[i] & -(data[i] >= 128);` - the shuffled run should become as fast as the sorted one.
- In `ilp.c`, try 2 and 8 accumulators. Where does the speedup stop growing? (That's the
  number of floating-point add units × their latency.)

## Real-world equivalent

- Pointer-heavy data structures (linked lists, trees, hash tables with chaining) pay the
  latency ladder on every hop - which is why B-trees use wide nodes and why `std::vector` beats
  `std::list` for almost everything.
- The famous Stack Overflow question *"Why is processing a sorted array faster than an
  unsorted array?"* is exactly `branch.c`.
- Spectre (2018) exploited the speculative execution that branch prediction relies on.
- Hash functions, compression libraries and JSON parsers (simdjson) are written with multiple
  independent accumulators and branchless code for exactly the reasons above.
