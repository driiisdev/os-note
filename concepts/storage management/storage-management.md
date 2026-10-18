# Storage Management - Section Intro

RAM forgets everything when the power goes out. **Storage** - SSDs and hard drives - doesn't,
but it's 1,000-100,000x slower than RAM and only speaks in **blocks**. Storage management is
how the OS turns that slow block device into **files and directories**, makes it feel fast
(the **page cache**), and lets programs control when data is truly safe (`fsync`).

## The layers between `write()` and the disk

```text
   application        write(fd, buf, n)
        |
   ----- syscall boundary ---------------------------------------------------
   VFS               virtual file system: one API (open/read/write/stat) for every FS
        |
   page cache        file data cached in RAM; writes land here and return
        |
   file system       ext4 / xfs / btrfs / NTFS: files -> inodes -> block numbers,
        |            directories, journaling, free-space tracking
   block layer       request merging, I/O scheduling, queues
        |
   device driver     NVMe / SATA / virtio: builds commands, sets up DMA
        |
   ----- hardware -----------------------------------------------------------
   device            SSD controller (FTL, flash) or HDD (platters, heads)
```

🖼️ **storage stack:**
![storage stack](<../../assets/img/storage-stack.png>)

## What this section covers

| # | Topic | The question it answers |
| --- | --- | --- |
| 1 | [Persistent storage](persistent-storage.md) | How do HDDs and SSDs work, and what does that mean for performance? |
| 2 | [File systems](file-systems.md) | How are files, directories and free space organized on a block device? |
| 3 | [What really happens in a file I/O?](what-really-happens-in-a-file-io.md) | The full journey of `read()` and `write()` - and when data is actually durable |
| 4 | [Storage management demo](storage-management-demo.md) | Watching the page cache, dirty pages and I/O on a real system |

## Key ideas to carry forward

- **Block devices** are read and written in fixed-size blocks (512B or 4KB sectors), never bytes.
- **The page cache** makes repeated reads fast and lets writes return immediately - at the cost
  of durability until data is flushed.
- **`write()` returning ≠ data on disk.** Only `fsync()`/`fdatasync()` (or `O_SYNC`/`O_DIRECT`
  with the right flags) guarantee that.
- **File systems are on-disk data structures** (superblock, inodes, bitmaps, directories) that
  must stay consistent across crashes - via journaling or copy-on-write.
- **Sequential beats random**, dramatically on HDDs and still noticeably on SSDs.

## Practice projects

- [`practice/storage-management/file-io-journey`](../../practice/storage-management/file-io-journey) -
  buffered vs `fsync` vs `O_DIRECT`, cold vs warm reads, crash-safe file replacement.
- [`practice/storage-management/toy-filesystem`](../../practice/storage-management/toy-filesystem) -
  a working inode-based file system inside a disk-image file.

## Related

- [DMA](<../memory management/dma.md>) - how the data actually moves between device and RAM.
- [Virtual memory](<../memory management/virtual-memory.md>) - `mmap`'d files and the page cache
  share the same machinery.
