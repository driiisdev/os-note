# Practice: Toy File System (`tinyfs`)

Concept notes:
[`file-systems.md`](<../../../concepts/storage management/file-systems.md>) ·
[`storage-management.md`](<../../../concepts/storage management/storage-management.md>)

## What this demonstrates

A real (tiny) inode-based file system that lives inside one ordinary 1MB file, a "disk image".
It uses the same building blocks as ext2/ext4, small enough to read in one sitting:

- a **superblock** that describes the layout,
- **bitmaps** that track which inodes and blocks are free,
- an **inode table**: one fixed-size record per file holding its type, size, link count and
  block pointers - but **not its name**,
- **directories** that are just files full of `(name, inode number)` records,
- **path resolution** that walks from the root one component at a time,
- **hard links**: two names, one inode.

Written in Python (no dependencies) because the interesting part is the on-disk data
structures, not the language.

## Folder structure

```text
toy-filesystem/
  tinyfs.py   <- the file system: mkfs, mkdir, write, cat, ls, ln, rm, stat, df
  demo.sh     <- runs every command on a fresh image, then hex-dumps a directory block
```

On-disk layout (1KB blocks, 1024 blocks):

```text
block 0        superblock     magic number + where every region starts
block 1        inode bitmap   1 bit per inode       (used / free)
block 2        block bitmap   1 bit per data block  (used / free)
blocks 3..10   inode table    128 inodes x 64 bytes
blocks 11..    data blocks    file contents and directory entries
```

## Build & run

```bash
bash demo.sh
# or by hand:
python3 tinyfs.py disk.img mkfs
python3 tinyfs.py disk.img mkdir /docs
echo "hi" | python3 tinyfs.py disk.img write /docs/a.txt
python3 tinyfs.py disk.img ls /docs
```

## What you'll actually see

```text
$ tinyfs mkfs
formatted: 1024 blocks x 1024 B, 128 inodes, data starts at block 11
$ tinyfs mkdir /docs
$ tinyfs write /docs/hello.txt
wrote 31 bytes to /docs/hello.txt (inode 3)
$ tinyfs ls /
d inode   1  links 1      96 B  .
d inode   1  links 1      96 B  ..
d inode   2  links 1      96 B  docs
$ tinyfs ls /docs
d inode   2  links 1      96 B  .
d inode   1  links 1      96 B  ..
- inode   3  links 1      31 B  hello.txt
$ tinyfs ln /docs/hello.txt /hi.txt
$ tinyfs stat /hi.txt
path   : /hi.txt
inode  : 3
type   : file
size   : 31 bytes
links  : 2
blocks : [13]
$ tinyfs rm /docs/hello.txt
$ tinyfs cat /hi.txt
hello from inside a disk image
$ tinyfs df
data blocks: 3/1013 used (3072 B)
inodes     : 4/128 used

$ xxd -s $((11*1024)) -l 96 disk.img   # first data block = the root directory's entries
00002c00: 0100 0000 2e00 0000 0000 0000 0000 0000  ................
00002c10: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00002c20: 0100 0000 2e2e 0000 0000 0000 0000 0000  ................
00002c30: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00002c40: 0200 0000 646f 6373 0000 0000 0000 0000  ....docs........
```

- **After `rm /docs/hello.txt`, `/hi.txt` still works** - `rm` only removed one *name*. The
  inode's link count dropped from 2 to 1, so its data stayed. That's what "unlink" means, and
  why the syscall is called `unlink()`, not `delete()`.
- **The hex dump is the root directory, raw**: `01 00 00 00` + `.` → inode 1; `01 00 00 00` +
  `..` → inode 1 (the root is its own parent); `02 00 00 00` + `docs` → inode 2. A directory
  really is just a file of `(inode, name)` records.

## Code walkthrough

**Path resolution** - `/docs/hello.txt` is resolved like the kernel's `namei`:

```python
def lookup(self, path):
    ino = ROOT                                    # start at inode 1
    for part in [p for p in path.split("/") if p]:
        found = dict(self.entries(ino)).get(part) # read the directory's (name, inode) list
        if found is None:
            raise FsError(f"{path}: no such file or directory")
        ino = found                               # descend
    return ino
```

**Reading a file** - follow the inode's block pointers, then trim to the recorded size:

```python
def read_data(self, node):
    out = bytearray()
    for b in node["blocks"]:          # 12 direct block numbers, 0 = unused
        if b:
            out += self.read_block(b)
    return bytes(out[:node["size"]])
```

**Allocation** - find the first 0 bit in a bitmap, flip it, write the bitmap block back:

```python
def alloc_bit(self, bitmap_block, limit):
    bm = self.read_block(bitmap_block)
    for i in range(limit):
        if not bm[i // 8] & (1 << (i % 8)):
            bm[i // 8] |= 1 << (i % 8)
            self.write_block(bitmap_block, bm)
            return i
```

**What's deliberately missing** (each one is a real file-system feature):

- **No crash safety.** Creating a file updates the inode bitmap, the inode, the block bitmap,
  a data block and the parent directory - five separate writes. Crash in between and the image
  is inconsistent (an allocated inode no directory points to = a leak; a directory entry
  pointing at a free inode = corruption). ext4 solves this with a **journal**; btrfs/ZFS with
  **copy-on-write**; old systems with `fsck` after every crash.
- **12 direct blocks only** → 12KB max file size. ext2 adds single, double and triple
  **indirect** blocks; ext4 uses **extents** ("blocks 5000-5999").
- **No subdirectory link counts, permissions, timestamps or caching.**

## Try this yourself

- Add a **single indirect block**: inode pointer 12 points to a block holding 256 more block
  numbers. Max file size goes from 12KB to 268KB.
- Add **mtime** to the inode (there's room - inodes are 64 bytes, only 56 are used).
- Implement `mv` - it should only touch directory entries, never the file's data blocks. (On
  the same file system, `rename()` is constant-time regardless of file size. Why?)
- Fill the inode table with 126 empty files (inode 0 is reserved, inode 1 is the root), then
  try to create one more while `df` still
  shows almost every block free: **inode exhaustion**, the real-world `df -i` problem.
- Simulate a crash: in `create()`, raise an exception right after `new_inode()` and before
  `add_entry()`. Then write an `fsck` command that finds the orphaned inode.

## Real-world equivalent

`mkfs.ext4` lays out exactly these structures (plus block groups, a journal, extents and a lot
of redundancy); `debugfs` lets you poke at them the way `tinyfs stat` does. `ls -i` shows inode
numbers on your real system; `stat file` shows link counts; `df -i` shows inode usage.
