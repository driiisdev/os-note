# What Really Happens in a File I/O?

`write(fd, buf, 4096)` returns in a microsecond or two. A storage device takes tens of
microseconds (SSD) to milliseconds (HDD) to store anything. So when `write()` returns, the data
**is not on the disk**. This note follows a read and a write all the way down, and explains what
you have to do to make data actually durable.

## The journey of a `read()`

```text
 read(fd, buf, 4096)                         user mode
 ------------------------------------------- syscall
 1. VFS: fd -> open file -> inode, current offset
 2. page cache lookup: is page (inode, offset/4096) in RAM?
       HIT  -> copy 4KB from the page cache into buf (copy_to_user)      ~1 µs
       MISS -> 3. file system: offset -> block number (inode's extents)
               4. block layer: build a request, maybe merge with others,
                  maybe add READAHEAD of the next pages
               5. driver: NVMe command, device DMAs data into a page-cache page
               6. thread sleeps (state D) until the completion interrupt   ~10-100 µs
               7. page now cached -> copy into buf
 8. advance the file offset, return bytes read
```

- **Readahead**: on sequential reads, the kernel detects the pattern and fetches upcoming pages
  before you ask, so most `read()`s are cache hits.
- **Cached pages stay** after the read - the next reader (any process) gets them from RAM.
  That's why the second `grep -r` over a directory is so much faster.

## The journey of a `write()`

```text
 write(fd, buf, 4096)
 ------------------------------------------- syscall
 1. VFS -> inode, offset
 2. find/create the page-cache page (read it first if only part is overwritten)
 3. copy buf into the page; mark it DIRTY
 4. update the file size / mtime in the in-memory inode (also dirty)
 5. RETURN  <-- the program continues here. nothing has touched the device.

 ...later, in the background (kernel flusher threads):
 6. dirty pages older than ~30s (dirty_expire_centisecs), or too many dirty
    pages overall (dirty_background_ratio / dirty_ratio), get written back
 7. file system allocates blocks, journal commits the metadata
 8. driver submits the writes; device DMAs data out of RAM
 9. device may still hold it in its VOLATILE write cache
```

🖼️ **Image to add - write path timeline:** a horizontal timeline from `write()` to "safe on
flash": `write()` returns after the page-cache copy (a few µs), then a gap of up to ~30s before
background writeback, then the device write, then the device cache flush - with `fsync()` drawn
as a shortcut that forces the whole sequence immediately and waits for it.
![write path](<../../assets/img/file-write-path.png>)

If the machine **loses power** between steps 5 and 9, the write is gone - even though the
program was told it succeeded. If only the *application* crashes, the data is fine: it's in the
kernel's page cache, which survives the process.

## Making data durable

| Call / flag | What it guarantees |
| --- | --- |
| `fsync(fd)` | the file's data **and** metadata reach stable storage, including a device cache flush. Blocks until done |
| `fdatasync(fd)` | data plus only the metadata needed to read it back (e.g. size) - skips mtime, slightly cheaper |
| `O_SYNC` / `O_DSYNC` at `open()` | every `write()` behaves like `write` + `fsync` / `fdatasync` |
| `O_DIRECT` | bypass the page cache: DMA straight between your (aligned) buffer and the device. **Not** a durability guarantee by itself - combine with `O_DSYNC` or `fsync` |
| `sync()` / `syncfs()` | flush everything / one file system |
| `msync()` | the same for `mmap`'d files |

**fsync is expensive** - it waits for the device, a journal commit, and a cache flush. In the
practice project: buffered writes ~700MB/s, `fsync` after every 4KB write ~1.8MB/s. That ~400x is
why databases use a **write-ahead log** and **group commit**: many transactions' log records,
one `fsync`.

## The atomic replace pattern

Overwriting a file in place isn't atomic - a crash can leave half the old content and half the
new. The standard fix:

```c
write(tmp_fd, data, len);   // 1. write the new version to a temp file in the same directory
fsync(tmp_fd);              // 2. make the temp file's data durable
rename("f.tmp", "f");       // 3. atomically swap which inode the name points to
fsync(dir_fd);              // 4. make the directory change (the rename) durable
```

Readers see either the complete old file or the complete new one. Skipping step 2 is a real
historical bug: ext4 could commit the rename before the data, leaving an empty file after a crash.

## Clarifications

- **"The OS is lying" is the wrong way to see it.** `write()` is defined as "the kernel has your
  data"; durability was always a separate call.
- **`close()` does not `fsync()`.** Closing a file doesn't force it to disk.
- **`fsync` errors matter.** If writeback fails (bad sector, full thin-provisioned disk), the error
  is reported by the next `fsync` - and, historically on Linux, possibly only once ("fsyncgate",
  2018). Postgres now treats an `fsync` failure as fatal and recovers from its WAL.
- **mmap'd writes are page-cache writes** too - they need `msync()` for durability.
- **Network file systems** (NFS, SMB, WSL's `/mnt/c`) have their own caching rules on top of this.

## Practice project

[`practice/storage-management/file-io-journey`](../../practice/storage-management/file-io-journey) -
`writepath` shows 64MB of dirty page-cache pages after buffered writes, and the cost of each
durability option; `readcache` uses `mincore()` to show 100% → 0% → 100% of a file in the page
cache around a cold read (17x slower than the warm one); `atomic-save` runs the four-step pattern.

## Related

- [Persistent storage](persistent-storage.md) - what's at the bottom of the path.
- [File systems](file-systems.md) - step 7 of the write path.
- [DMA](<../memory management/dma.md>) - step 5 of the read path.
- [Storage management demo](storage-management-demo.md) - watching dirty pages and writeback live.
