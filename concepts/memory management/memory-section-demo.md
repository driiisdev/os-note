# Memory Section Demo

The lecture demo puts the memory-management theory in front of real tools. These notes are a
reproducible version - the specific steps from the lecture can be added here from the `.txt`.
Everything below works in WSL2 or any Linux box, without root.

## 1. How much memory is there, and where is it?

```bash
free -h                         # total / used / free / buff/cache / available
cat /proc/meminfo | head -20    # the kernel's own accounting
```

- **`buff/cache`** is RAM the kernel is using for the **page cache** (file data) - it's
  reclaimable, so **`available`**, not `free`, is the number that answers "how much can
  programs still get?". Linux tries to use all "free" RAM as cache; low `free` is normal.
- `Dirty:` in `/proc/meminfo` is file data written by programs but not yet flushed to disk.
- `PageTables:` is how much RAM is spent on page tables, system-wide.

## 2. One process: virtual vs resident

```bash
grep -E 'VmSize|VmRSS|VmPTE|VmSwap' /proc/<pid>/status
ps -o pid,vsz,rss,cmd -p <pid>
pmap -x <pid> | tail -1
```

- **VSZ / VmSize** - all virtual memory mapped (reserved).
- **RSS / VmRSS** - pages actually in RAM right now.
- **VmPTE** - this process's page tables.

A process with VmSize in GB and RSS in MB is normal: reserved address space is free until touched.

## 3. Page faults, live

```bash
/usr/bin/time -v ls -R /usr > /dev/null    # "Minor (reclaiming a frame) page faults: ..."
ps -o pid,min_flt,maj_flt,cmd -p <pid>     # cumulative faults per process
```

Run something twice: the second run usually has far fewer **major** faults, because the
program's files are now in the page cache.

## 4. Watch demand paging and copy-on-write

From the practice project:

```bash
cd practice/memory-management/virtual-memory-lab && make demo
```

```text
  after mmap(256MB)      RSS    1080 KB   page tables (VmPTE)    44 KB   minor faults      61
  after writing each pg  RSS  263664 KB   page tables (VmPTE)   556 KB   minor faults   65600
  4KB pages, write: 65539 faults for 65536 pages touched, 180.7 ms total (~2.76 us per fault)
```

`mmap` changed nothing; RSS and page tables grew one fault at a time as pages were touched.

## 5. Watch the caches

```bash
cd practice/memory-management/cache-locality && make && ./bin/locality
```

```text
   row-major   :     9.3 ms
   column-major:   180.2 ms   (19.4x slower, same 16777216-element sum)
```

🖼️ **free -h explained:**
![free -h explained](<../../assets/img/free-h-explained.png>)

## What to take away

- **Free RAM is wasted RAM** - the kernel fills it with page cache and gives it back on demand.
- **Virtual size is mostly a promise**; resident size is what costs RAM.
- **Page faults are the normal mechanism**, not an error - minor faults are cheap, major
  faults (disk) are what hurt.
- **Access patterns matter more than instruction counts** once data doesn't fit in cache.

## Practice projects

- [`practice/memory-management/virtual-memory-lab`](../../practice/memory-management/virtual-memory-lab)
- [`practice/memory-management/cache-locality`](../../practice/memory-management/cache-locality)

## Related

- [Virtual memory](virtual-memory.md) - the theory behind steps 2-4.
- [Reading and writing from and to memory](reading-n-writing-from-n-to-memory.md) - the theory
  behind step 5.
- [Storage management demo](<../storage management/storage-management-demo.md>) - the page cache
  from the file side.
