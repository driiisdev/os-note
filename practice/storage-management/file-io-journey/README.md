# Practice: The Journey of a File I/O

Concept notes:
[`what-really-happens-in-a-file-io.md`](<../../../concepts/storage management/what-really-happens-in-a-file-io.md>) ·
[`persistent-storage.md`](<../../../concepts/storage management/persistent-storage.md>) ·
[`storage-management-demo.md`](<../../../concepts/storage management/storage-management-demo.md>) ·
[`dma.md`](<../../../concepts/memory management/dma.md>)

## What this demonstrates

That `write()` returning does **not** mean your data is on disk, and what it actually costs to
make it so.

1. **`writepath`** - the same 4KB writes four ways: plain buffered, buffered + one `fsync()`,
   `O_DIRECT` (bypass the page cache), and `fsync()` after every write.
2. **`readcache`** - the page cache on the read side: a cold read (from the device) vs a warm
   read (from RAM), with `mincore()` asking the kernel exactly how much of the file it's holding.
3. **`atomic-save`** - the write-temp → fsync → rename → fsync-directory pattern that makes
   replacing a file crash-safe.

## Folder structure

```text
file-io-journey/
  writepath.c     <- buffered vs fsync vs O_DIRECT vs fsync-per-write
  readcache.c     <- cold vs warm reads, page-cache residency with mincore()
  atomic-save.c   <- crash-safe file replacement
  Makefile        <- `make`, `make demo`, `make watch-dirty`
```

**Run it on a real Linux filesystem** (e.g. your home directory inside WSL, `~/`), not on
`/mnt/c/...`. Windows drives are mounted through a network-style filesystem that doesn't
support `O_DIRECT` and has its own caching, which would make every number here meaningless.

## Build & run

```bash
cp -r . ~/file-io-journey && cd ~/file-io-journey    # if you're on /mnt/c
make demo
make watch-dirty     # in a 2nd terminal, while running ./bin/writepath
```

## What you'll actually see

On a WSL2 virtual disk (backed by an NVMe SSD on the Windows side):

```text
4KB writes to testfile.bin
  buffered      64 MB  write() calls done after    91.6 ms, durable: NOT YET      698.5 MB/s  (page cache dirty: +65536 KB)
  fsync-end     64 MB  write() calls done after    44.8 ms, durable after   266.1 ms     240.5 MB/s
  direct        64 MB  write() calls done after  3802.3 ms, durable after  3802.3 ms      16.8 MB/s
  fsync-each     4 MB  write() calls done after  2251.1 ms, durable after  2251.1 ms       1.8 MB/s

just written           : 100.0% of the file is in the page cache
after FADV_DONTNEED    :   0.0% cached
cold read (from disk)  :   374.5 ms      342 MB/s   -> now 100.0% cached
warm read (from RAM)   :    22.7 ms     5645 MB/s   (17x faster)

saving v1:
  1. wrote 17 bytes to ./app-config.json.tmp       (only in the page cache so far)
  2. fsync(./app-config.json.tmp)                   (data is on the device)
  3. rename(./app-config.json.tmp -> ./app-config.json)  (atomic directory update)
  4. fsync(directory)                         (the rename is durable too)
...
final content: { "version": 2, "feature": true }
```

- **Buffered: 64MB "written" in 92ms, and 64MB of dirty pages sitting in RAM.** Pull the power
  now and it's gone. The kernel's flusher threads will write it back on their own schedule
  (by default, dirty data older than ~30s, checked every 5s).
- **fsync-end** pays for durability once, at the end: the 64MB goes to the device in big,
  merged writes.
- **O_DIRECT, 4KB at a time: 17MB/s.** No cache to absorb anything - every 4KB `write()` is a
  separate device I/O, DMA'd straight from the user buffer, and the call waits for it.
- **fsync after every write: 1.8MB/s - ~400x slower than buffered.** Every 4KB also forces a
  journal commit and a device cache flush. This is the cost a database pays per commit, and
  why databases batch commits (group commit) and use a write-ahead log.
- **Cold vs warm: 17x.** The warm read never touches the device - it's a `memcpy` from the page
  cache.

## Code walkthrough

```c
// O_DIRECT: buffer, file offset and length must all be aligned to the device block size
int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
posix_memalign(&buf, 4096, CHUNK);

// what "durable" means: wait until the device says the data (and metadata) is stored
fsync(fd);
```

```c
// evict ONE file's clean pages from the page cache - no root, no global drop_caches
fsync(fd);                                   // pages must be clean before they can be dropped
posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);

// ask the kernel which pages of a mapping are resident in RAM
mincore(map, SIZE, vec);                     // vec[i] & 1 == page i is cached
```

```c
write(tmp_fd, data, len);   fsync(tmp_fd);   // 1-2: new content durable under a temp name
rename(tmp, path);                           // 3: atomic: readers see old OR new, never half
fsync(dir_fd);                               // 4: make the directory change durable too
```

Skipping step 2 is the classic bug: after a crash, ext4 can end up with the *new name*
pointing at a file whose *data* never made it to disk - an empty or truncated config file.

## Try this yourself

- Run `./bin/writepath` while `make watch-dirty` is open in another terminal - watch `Dirty:`
  jump by 64MB and then drain over the next ~30 seconds as the kernel writes it back.
- Change `CHUNK` to 1MB for the `direct` mode. With fewer, larger I/Os, O_DIRECT gets much
  closer to `fsync-end` - per-I/O overhead, not bandwidth, was the bottleneck.
- `cat /proc/sys/vm/dirty_expire_centisecs /proc/sys/vm/dirty_writeback_centisecs /proc/sys/vm/dirty_ratio`
  - the knobs that decide when buffered writes become real writes.
- `strace -e trace=openat,write,fsync,rename ./bin/atomic-save` to see the exact syscall sequence.

## Real-world equivalent

- **Postgres** writes its WAL and calls `fsync` (or `fdatasync`) at commit;
  `synchronous_commit = off` trades the last few hundred ms of commits for speed - the
  `buffered` vs `fsync-each` gap. The 2018 "fsyncgate" was about what happens when `fsync`
  *fails*.
- **MySQL/InnoDB** uses `O_DIRECT` (`innodb_flush_method=O_DIRECT`) so the database's own
  buffer pool isn't duplicated in the OS page cache.
- **Kafka** deliberately leans on the page cache and sequential writes instead of fsyncing
  each message, relying on replication for durability.
- Text editors, `git`, package managers and SQLite's journal all use the rename-for-atomicity
  pattern from `atomic-save.c`.
