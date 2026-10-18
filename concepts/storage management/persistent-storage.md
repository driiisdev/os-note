# Persistent Storage

Persistent storage keeps data without power. The two technologies that dominate - **spinning
hard drives (HDDs)** and **flash-based solid-state drives (SSDs)** - have completely different
physics, and that difference explains a lot of OS and database design.

## Hard disk drives (HDD)

```text
      spindle (5,400 - 15,000 RPM)
        |
   =====O=====  platter (both sides magnetic)      <- data in concentric TRACKS,
        |          ^                                   each split into SECTORS
   =====O=====     | read/write head on an arm
        |          | (moves in and out = SEEK)
```

Reading a sector costs:

1. **Seek time** - move the head to the right track: ~3-10ms.
2. **Rotational latency** - wait for the sector to spin under the head: on average half a
   revolution, ~4ms at 7,200 RPM.
3. **Transfer time** - read it: tiny by comparison.

So a **random** 4KB read costs ~10ms → ~100 random IOPS. A **sequential** read just keeps
streaming as the platter spins → 150-250MB/s. That ~1000x gap between random and sequential is
why databases use B-trees with big pages, why logs are append-only, and why old file systems
worked hard to keep files contiguous.

## Solid-state drives (SSD)

No moving parts: data is stored as charge in **NAND flash** cells, organized as:

- **pages** (typically 4-16KB) - the unit of **read** and **program** (write);
- **erase blocks** (hundreds of pages, several MB) - the unit of **erase**.

The catch: **a flash page can't be overwritten in place.** It must be erased first, and erases
only happen a whole block at a time. So the SSD's controller runs a **flash translation layer
(FTL)**:

- writes always go to a fresh, already-erased page; the old page is marked stale;
- a **logical-to-physical mapping table** remembers where each logical block currently lives
  (exactly like a page table);
- **garbage collection** copies the still-valid pages out of mostly-stale blocks and erases them;
- **wear leveling** spreads writes evenly, because each cell survives only a limited number of
  program/erase cycles (thousands for TLC/QLC flash);
- **TRIM / discard** - the OS tells the SSD which blocks the file system no longer uses, so GC
  doesn't waste effort preserving deleted data.

The extra internal copying is **write amplification**: 1MB written by the OS may cause several MB
of actual flash writes - worse with small random writes and a nearly full drive.

🖼️ **HDD vs SSD internals:**
![HDD vs SSD](<../../assets/img/hdd-vs-ssd.png>)

## Interfaces: how the OS talks to them

| | SATA / AHCI | NVMe |
| --- | --- | --- |
| Designed for | hard drives | flash |
| Bus | SATA (~600MB/s max) | PCIe (several GB/s per device) |
| Command queues | 1 queue, 32 commands | up to 64K queues × 64K commands - typically one queue pair per CPU core |
| Latency overhead | higher | very low |

NVMe's per-core queues mean cores can submit I/O without locking against each other - the
software stack, not the device, became the bottleneck, which pushed Linux toward multi-queue
block I/O (`blk-mq`) and `io_uring`.

## Comparing the numbers (orders of magnitude)

| | HDD | SATA SSD | NVMe SSD |
| --- | --- | --- | --- |
| random 4KB read latency | ~5-10 ms | ~100 µs | ~10-100 µs |
| random read IOPS | ~100-200 | ~50-100K | ~500K-1M+ |
| sequential throughput | ~150-250 MB/s | ~550 MB/s | ~3-14 GB/s |

## Durability details that matter to the OS

- **Logical block addressing (LBA)** - the OS sees a flat array of numbered blocks (sectors of
  512 bytes or 4KB); the device maps them to physical locations.
- **Volatile write caches** - most drives acknowledge writes once they're in the drive's own
  DRAM cache. Data is only safe after a **cache flush** command, which is part of what `fsync()`
  triggers. Enterprise SSDs have **power-loss protection** (capacitors) to make their cache safe.
- **Atomicity** - a device generally guarantees that a single sector write is either fully done
  or not done, but not that a larger multi-sector write is all-or-nothing. File systems and
  databases build their own atomicity on top (journaling, write-ahead logs, checksums).

## Clarifications

- **SSDs still prefer sequential writes** - large sequential writes cause less garbage
  collection and write amplification than small random ones.
- **"Disk" is used loosely** for any block device, including SSDs and virtual disks (like WSL2's,
  which is a file on the Windows drive).
- **Storage ≠ memory**, even when it's fast: it's accessed through a driver in blocks, not by CPU
  load/store instructions.

## Practice project

[`practice/storage-management/file-io-journey`](../../practice/storage-management/file-io-journey) -
`O_DIRECT` 4KB writes (~17MB/s) vs buffered writes (~700MB/s) vs an `fsync` after every write
(~1.8MB/s) shows per-I/O device cost, the cost of flushing the device cache, and why batching
matters.

## Related

- [File systems](file-systems.md) - the structure built on top of the block device.
- [What really happens in a file I/O?](what-really-happens-in-a-file-io.md) - the path from
  `write()` to the device.
- [DMA](<../memory management/dma.md>) - how the device moves data in and out of RAM.
