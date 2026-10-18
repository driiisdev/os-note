# Storage Management Demo

The lecture demo shows the storage stack doing its work on a real machine: disks and file
systems, the page cache absorbing reads and writes, and the cost of forcing data to disk. Below
is a reproducible version (WSL2 or any Linux box); the lecture's exact steps can be added from
the `.txt`.

> In WSL2, run these from your Linux home directory (`~`), which is an ext4 virtual disk.
> `/mnt/c` is the Windows drive mounted through a file-sharing protocol, with different caching.

## 1. What storage is there?

```bash
lsblk -o NAME,SIZE,ROTA,TYPE,MOUNTPOINT,FSTYPE   # ROTA=1 rotational (HDD), 0 = SSD
df -hT                                         # file systems, types, usage
df -i                                          # inode usage - can run out before space does
cat /sys/block/sda/queue/scheduler             # the block-layer I/O scheduler
```

## 2. Files are inodes

```bash
echo hi > a.txt
ls -li a.txt                    # the first column is the inode number
stat a.txt                      # size, blocks, links, timestamps, inode
ln a.txt b.txt; ls -li a.txt b.txt   # same inode, link count 2
rm a.txt; cat b.txt             # still there - one name removed, not the file
```

## 3. The page cache, on the read side

```bash
free -h                                   # note buff/cache
dd if=/dev/zero of=big.bin bs=1M count=512 && sync
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'   # drop clean caches (system-wide, needs root)
time cat big.bin > /dev/null              # cold: from the device
time cat big.bin > /dev/null              # warm: from RAM - many times faster
free -h                                   # buff/cache grew by ~512MB
```

(No root? The practice project's `readcache` evicts just one file with `posix_fadvise`.)

## 4. The page cache, on the write side

```bash
watch -n 0.5 "grep -E '^(Dirty|Writeback):' /proc/meminfo"   # in one terminal
dd if=/dev/zero of=big2.bin bs=1M count=512                    # in another: Dirty jumps...
# ...and drains over the next ~30 seconds as the kernel writes it back
dd if=/dev/zero of=big3.bin bs=1M count=512 conv=fsync        # same, but waits for the disk
dd if=/dev/zero of=big4.bin bs=4k count=2000 oflag=dsync      # sync every write: very slow
rm big*.bin
```

## 5. Watching I/O as it happens

```bash
iostat -xz 1        # per-device: r/s, w/s, MB/s, await (latency), %util   (sysstat package)
vmstat 1            # bi/bo = blocks in/out, wa = iowait
sudo iotop          # per-process I/O
```

🖼️ **dirty pages draining:**
![dirty pages](<../../assets/img/dirty-pages.png>)

## From the practice project

```bash
cd practice/storage-management/file-io-journey && make demo
```

```text
  buffered      64 MB  write() calls done after    91.6 ms, durable: NOT YET      698.5 MB/s  (page cache dirty: +65536 KB)
  fsync-end     64 MB  write() calls done after    44.8 ms, durable after   266.1 ms     240.5 MB/s
  direct        64 MB  write() calls done after  3802.3 ms, durable after  3802.3 ms      16.8 MB/s
  fsync-each     4 MB  write() calls done after  2251.1 ms, durable after  2251.1 ms       1.8 MB/s

cold read (from disk)  :   374.5 ms      342 MB/s   -> now 100.0% cached
warm read (from RAM)   :    22.7 ms     5645 MB/s   (17x faster)
```

## What to take away

- **Free RAM fills up with page cache** - that's a feature, and it's reclaimable.
- **Buffered writes are fast because they're not done yet.** Durability is a separate, expensive
  step you opt into.
- **Small synchronous writes are the worst case** for any storage device - batch them.
- **Names and files are separate** - inodes are the files; directories just name them.

## Practice projects

- [`practice/storage-management/file-io-journey`](../../practice/storage-management/file-io-journey)
- [`practice/storage-management/toy-filesystem`](../../practice/storage-management/toy-filesystem) -
  step 2's inode/link behavior, implemented from scratch.

## Related

- [What really happens in a file I/O?](what-really-happens-in-a-file-io.md) - the theory behind
  steps 3 and 4.
- [File systems](file-systems.md) - the theory behind step 2.
- [Memory section demo](<../memory management/memory-section-demo.md>) - the page cache from the
  memory side (`buff/cache`).
