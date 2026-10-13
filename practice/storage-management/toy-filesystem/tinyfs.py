#!/usr/bin/env python3
"""tinyfs - a tiny inode-based file system that lives inside one ordinary file (a disk image).

It uses the same building blocks as ext2/ext4, just smaller: a superblock, two bitmaps,
an inode table, and data blocks.

    block 0        superblock     magic number + where every region starts
    block 1        inode bitmap   1 bit per inode       (used / free)
    block 2        block bitmap   1 bit per data block  (used / free)
    blocks 3..10   inode table    128 inodes x 64 bytes
    blocks 11..    data blocks    file contents and directory entries

usage:
    python3 tinyfs.py disk.img mkfs
    python3 tinyfs.py disk.img mkdir /docs
    echo "hello" | python3 tinyfs.py disk.img write /docs/hello.txt
    python3 tinyfs.py disk.img write /notes.txt some-host-file.txt
    python3 tinyfs.py disk.img ls /docs
    python3 tinyfs.py disk.img cat /docs/hello.txt
    python3 tinyfs.py disk.img ln /docs/hello.txt /hi.txt      # hard link
    python3 tinyfs.py disk.img stat /hi.txt
    python3 tinyfs.py disk.img rm /docs/hello.txt
    python3 tinyfs.py disk.img df
"""
import struct
import sys

BLOCK = 1024              # bytes per block
NBLOCKS = 1024            # 1MB disk image
NINODES = 128
INODE_SIZE = 64
INODE_BITMAP, BLOCK_BITMAP, INODE_TABLE = 1, 2, 3
DATA_START = INODE_TABLE + NINODES * INODE_SIZE // BLOCK   # = 11
DIRECT = 12               # direct block pointers per inode -> max file size 12KB
ROOT = 1                  # inode 0 is reserved to mean "empty directory slot"
MAGIC = 0x7F5F5001

FREE, FILE, DIR = 0, 1, 2
# inode on disk: type, link count, size in bytes, 12 direct block numbers (0 = unused)
INODE_FMT = "<HHI" + "I" * DIRECT
# directory entry on disk: inode number + name, 32 bytes, 32 entries per block
DIRENT_FMT = "<I28s"
DIRENT_SIZE = struct.calcsize(DIRENT_FMT)


class FsError(Exception):
    pass


class TinyFS:
    def __init__(self, image):
        self.f = open(image, "r+b")

    # ---- raw block device -----------------------------------------------------------------
    def read_block(self, n):
        self.f.seek(n * BLOCK)
        return bytearray(self.f.read(BLOCK))

    def write_block(self, n, data):
        assert len(data) <= BLOCK
        self.f.seek(n * BLOCK)
        self.f.write(bytes(data).ljust(BLOCK, b"\0"))

    # ---- bitmaps: find a 0 bit, flip it to 1 ----------------------------------------------
    def alloc_bit(self, bitmap_block, limit):
        bm = self.read_block(bitmap_block)
        for i in range(limit):
            if not bm[i // 8] & (1 << (i % 8)):
                bm[i // 8] |= 1 << (i % 8)
                self.write_block(bitmap_block, bm)
                return i
        raise FsError("no space left on device")

    def free_bit(self, bitmap_block, i):
        bm = self.read_block(bitmap_block)
        bm[i // 8] &= ~(1 << (i % 8))
        self.write_block(bitmap_block, bm)

    def count_used(self, bitmap_block, limit):
        bm = self.read_block(bitmap_block)
        return sum(1 for i in range(limit) if bm[i // 8] & (1 << (i % 8)))

    def alloc_block(self):
        n = DATA_START + self.alloc_bit(BLOCK_BITMAP, NBLOCKS - DATA_START)
        self.write_block(n, b"")  # never hand out a block with someone else's old data in it
        return n

    def free_block(self, n):
        self.free_bit(BLOCK_BITMAP, n - DATA_START)

    # ---- inodes ---------------------------------------------------------------------------
    def read_inode(self, ino):
        blk, off = divmod(ino * INODE_SIZE, BLOCK)
        raw = self.read_block(INODE_TABLE + blk)[off:off + struct.calcsize(INODE_FMT)]
        t, links, size, *blocks = struct.unpack(INODE_FMT, raw)
        return {"type": t, "links": links, "size": size, "blocks": blocks}

    def write_inode(self, ino, node):
        blk, off = divmod(ino * INODE_SIZE, BLOCK)
        data = self.read_block(INODE_TABLE + blk)
        packed = struct.pack(INODE_FMT, node["type"], node["links"], node["size"], *node["blocks"])
        data[off:off + len(packed)] = packed
        self.write_block(INODE_TABLE + blk, data)

    def new_inode(self, kind):
        ino = self.alloc_bit(INODE_BITMAP, NINODES)
        self.write_inode(ino, {"type": kind, "links": 1, "size": 0, "blocks": [0] * DIRECT})
        return ino

    # ---- file contents: follow the inode's block pointers ---------------------------------
    def read_data(self, node):
        out = bytearray()
        for b in node["blocks"]:
            if b:
                out += self.read_block(b)
        return bytes(out[:node["size"]])

    def write_data(self, ino, data):
        if len(data) > DIRECT * BLOCK:
            raise FsError(f"file too large: max {DIRECT * BLOCK} bytes (12 direct blocks)")
        node = self.read_inode(ino)
        for b in node["blocks"]:          # simplest possible overwrite: free, then reallocate
            if b:
                self.free_block(b)
        node["blocks"] = [0] * DIRECT
        for i in range(0, len(data), BLOCK):
            b = self.alloc_block()
            self.write_block(b, data[i:i + BLOCK])
            node["blocks"][i // BLOCK] = b
        node["size"] = len(data)
        self.write_inode(ino, node)

    # ---- directories: just files whose contents are (inode, name) records -----------------
    def entries(self, dir_ino):
        data = self.read_data(self.read_inode(dir_ino))
        for off in range(0, len(data), DIRENT_SIZE):
            ino, name = struct.unpack(DIRENT_FMT, data[off:off + DIRENT_SIZE])
            if ino:
                yield name.rstrip(b"\0").decode(), ino

    def add_entry(self, dir_ino, name, ino):
        if len(name.encode()) > 27:
            raise FsError("name too long (max 27 bytes)")
        data = bytearray(self.read_data(self.read_inode(dir_ino)))
        rec = struct.pack(DIRENT_FMT, ino, name.encode())
        for off in range(0, len(data), DIRENT_SIZE):   # reuse a slot freed by rm
            if struct.unpack_from("<I", data, off)[0] == 0:
                data[off:off + DIRENT_SIZE] = rec
                break
        else:
            data += rec
        self.write_data(dir_ino, bytes(data))

    def remove_entry(self, dir_ino, name):
        data = bytearray(self.read_data(self.read_inode(dir_ino)))
        for off in range(0, len(data), DIRENT_SIZE):
            ino, raw = struct.unpack(DIRENT_FMT, data[off:off + DIRENT_SIZE])
            if ino and raw.rstrip(b"\0").decode() == name:
                data[off:off + DIRENT_SIZE] = b"\0" * DIRENT_SIZE
                self.write_data(dir_ino, bytes(data))
                return
        raise FsError(f"{name}: not found")

    # ---- path resolution: walk from the root one component at a time ----------------------
    def lookup(self, path):
        ino = ROOT
        for part in [p for p in path.split("/") if p]:
            if self.read_inode(ino)["type"] != DIR:
                raise FsError(f"{part}: parent is not a directory")
            found = dict(self.entries(ino)).get(part)
            if found is None:
                raise FsError(f"{path}: no such file or directory")
            ino = found
        return ino

    def split(self, path):
        parts = [p for p in path.split("/") if p]
        if not parts:
            raise FsError("can't use / here")
        return self.lookup("/" + "/".join(parts[:-1])), parts[-1]

    # ---- commands -------------------------------------------------------------------------
    def mkfs(self):
        self.f.truncate(0)
        self.f.truncate(NBLOCKS * BLOCK)
        self.write_block(0, struct.pack("<IIIII", MAGIC, NBLOCKS, NINODES, INODE_TABLE, DATA_START))
        self.alloc_bit(INODE_BITMAP, NINODES)          # inode 0: reserved
        root = self.new_inode(DIR)                      # inode 1: the root directory
        self.add_entry(root, ".", root)
        self.add_entry(root, "..", root)
        print(f"formatted: {NBLOCKS} blocks x {BLOCK} B, {NINODES} inodes, data starts at block {DATA_START}")

    def check(self):
        if struct.unpack("<I", self.read_block(0)[:4])[0] != MAGIC:
            raise FsError("not a tinyfs image - run mkfs first")

    def create(self, path, kind):
        parent, name = self.split(path)
        if name in dict(self.entries(parent)):
            raise FsError(f"{path}: already exists")
        ino = self.new_inode(kind)
        if kind == DIR:
            self.add_entry(ino, ".", ino)
            self.add_entry(ino, "..", parent)
        self.add_entry(parent, name, ino)
        return ino

    def cmd_mkdir(self, path):
        self.create(path, DIR)

    def cmd_write(self, path, host_file=None):
        data = open(host_file, "rb").read() if host_file else sys.stdin.buffer.read()
        try:
            ino = self.lookup(path)
        except FsError:
            ino = self.create(path, FILE)
        self.write_data(ino, data)
        print(f"wrote {len(data)} bytes to {path} (inode {ino})")

    def cmd_cat(self, path):
        node = self.read_inode(self.lookup(path))
        if node["type"] != FILE:
            raise FsError(f"{path}: is a directory")
        sys.stdout.buffer.write(self.read_data(node))

    def cmd_ls(self, path="/"):
        for name, ino in self.entries(self.lookup(path)):
            node = self.read_inode(ino)
            kind = "d" if node["type"] == DIR else "-"
            print(f"{kind} inode {ino:3}  links {node['links']}  {node['size']:6} B  {name}")

    def cmd_stat(self, path):
        ino = self.lookup(path)
        node = self.read_inode(ino)
        used = [b for b in node["blocks"] if b]
        print(f"path   : {path}\ninode  : {ino}\ntype   : {'directory' if node['type'] == DIR else 'file'}")
        print(f"size   : {node['size']} bytes\nlinks  : {node['links']}\nblocks : {used or '(none)'}")

    def cmd_ln(self, target, link):
        ino = self.lookup(target)
        if self.read_inode(ino)["type"] == DIR:
            raise FsError("hard links to directories are not allowed")
        parent, name = self.split(link)
        self.add_entry(parent, name, ino)               # a second name for the SAME inode
        node = self.read_inode(ino)
        node["links"] += 1
        self.write_inode(ino, node)

    def cmd_rm(self, path):
        parent, name = self.split(path)
        ino = self.lookup(path)
        node = self.read_inode(ino)
        if node["type"] == DIR and any(n not in (".", "..") for n, _ in self.entries(ino)):
            raise FsError(f"{path}: directory not empty")
        self.remove_entry(parent, name)
        node["links"] -= 1
        if node["links"] > 0:                           # other names still point at it
            self.write_inode(ino, node)
            return
        for b in node["blocks"]:                        # last name gone: free everything
            if b:
                self.free_block(b)
        self.free_bit(INODE_BITMAP, ino)
        self.write_inode(ino, {"type": FREE, "links": 0, "size": 0, "blocks": [0] * DIRECT})

    def cmd_df(self):
        data_blocks = NBLOCKS - DATA_START
        used_b = self.count_used(BLOCK_BITMAP, data_blocks)
        used_i = self.count_used(INODE_BITMAP, NINODES)
        print(f"data blocks: {used_b}/{data_blocks} used ({used_b * BLOCK} B)")
        print(f"inodes     : {used_i}/{NINODES} used")


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    image, cmd, args = argv[1], argv[2], argv[3:]
    if cmd == "mkfs":
        open(image, "ab").close()
        TinyFS(image).mkfs()
        return 0
    fs = TinyFS(image)
    try:
        fs.check()
        handler = getattr(fs, "cmd_" + cmd, None)
        if handler is None:
            raise FsError(f"unknown command: {cmd}")
        handler(*args)
    except (FsError, TypeError) as e:
        print(f"tinyfs: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
