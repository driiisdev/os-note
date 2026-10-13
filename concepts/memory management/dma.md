# DMA (Direct Memory Access)

When 1GB arrives from an NVMe SSD or over a 100Gbit network link, the CPU doesn't copy it into
memory byte by byte. The **device itself** writes the data straight into RAM - **direct memory
access** - and only taps the CPU on the shoulder (an **interrupt**) when it's done. DMA is why
modern storage and networking can run at full speed without consuming entire CPU cores.

## Without DMA: programmed I/O

In **programmed I/O (PIO)**, the CPU moves every word itself:

```text
loop:
    wait until device status register says "data ready"   ; polling, or one interrupt per word
    read one word from the device's data register
    store it into RAM
    repeat ~250 million times for 1GB
```

The CPU is fully occupied doing nothing but copying. Very old disks (and some simple
microcontroller peripherals today) worked this way.

## With DMA

```text
   CPU                         RAM                          device (NVMe / NIC)
    |  1. driver writes a descriptor:                              |
    |     "read 1MB from LBA 2048 into physical address 0x3a400000"|
    |------------------------------------------------------------->|  (via memory-mapped registers
    |                                                              |   / a submission queue in RAM)
    |  2. CPU goes off and runs other threads                      |
    |                            <======== 3. device bus-masters over PCIe,
    |                                         writes 1MB directly into RAM
    |  <----------------------------------------- 4. completion + interrupt
    |  5. driver marks the I/O done, wakes the waiting thread
```

- The device becomes a **bus master** on PCIe and issues its own memory reads/writes.
- Modern devices take work from **ring buffers / queues** in RAM: NVMe has submission and
  completion queues (up to 64K queues, one or more per core); NICs have RX and TX descriptor
  rings.
- **Interrupt moderation / coalescing** batches completions so the CPU isn't interrupted
  per packet. Linux's network stack also switches to **polling** (NAPI) under heavy load.

🖼️ **DMA transfer:**
![DMA transfer](<../../assets/img/dma-transfer.png>)

## Where DMA shows up in the rest of the course

- **Reading a file** ([what really happens in a file I/O](<../storage management/what-really-happens-in-a-file-io.md>)):
  the SSD DMAs blocks into the **page cache**; then `read()` *copies* them from the page cache
  into your buffer (`copy_to_user`). With `O_DIRECT`, the device DMAs straight into **your**
  buffer instead - which is why `O_DIRECT` buffers must be aligned.
- **Receiving a packet** ([sending and receiving data](<../socket management/sending-n-receiving-data.md>)):
  the NIC DMAs frames into pre-allocated RX ring buffers, raises an interrupt, and the kernel's
  network stack takes it from there into the socket's receive buffer.
- **Sending** - the NIC DMAs packet data *out of* RAM; `sendfile()` and `MSG_ZEROCOPY` let it
  DMA straight from the page cache or user pages, avoiding copies.

## The complications

- **Physical addresses**: devices don't go through the CPU's MMU, so classically they needed
  physical addresses - and the buffers had to be **pinned** (not swapped out or moved) for the
  duration of the transfer.
- **IOMMU** (Intel VT-d, AMD-Vi): gives devices their own page tables, so the OS can (a) present
  scattered physical pages as one contiguous range to the device and (b) restrict each device
  to the memory it's supposed to touch. Without one, a malicious device (e.g. over Thunderbolt)
  can read all of RAM - a "DMA attack". The IOMMU is also what makes passing a physical device
  through to a VM safe.
- **Cache coherence**: the CPU might hold stale cached copies of a buffer the device just wrote.
  On x86, PCIe DMA is cache-coherent in hardware; on some ARM systems the driver must flush or
  invalidate caches explicitly. Linux hides this behind the DMA mapping API
  (`dma_map_single`, `dma_alloc_coherent`).

## Clarifications

- **DMA doesn't make I/O faster per byte; it frees the CPU.** The device moves data at its own
  speed; the CPU spends that time on other work instead of copying.
- **DMA ≠ zero-copy.** The device→RAM step is always DMA on modern hardware. "Zero-copy" means
  avoiding the *additional* CPU copies (page cache → user buffer → socket buffer) on top of it.
- **RDMA** (Remote DMA, e.g. InfiniBand, RoCE) extends the idea across the network: one
  machine's NIC writes directly into another machine's memory, bypassing both kernels.
- **GPUs** use the same mechanism to move textures, model weights and results in and out of
  their own memory.

## Practice project

DMA happens inside drivers, so there's no user-space program that "does DMA". You can see its
effects:

- [`practice/storage-management/file-io-journey`](../../practice/storage-management/file-io-journey) -
  the `direct` mode uses `O_DIRECT`, where the device DMAs straight to/from the program's
  4KB-aligned buffer (no page-cache copy); the cold vs warm read shows the cost of the device
  DMA vs a RAM-to-RAM copy.
- [`practice/os-into/hello-three-ways`](../../practice/os-into/hello-three-ways) - `sysinfo.sh`
  lists the busiest interrupt lines; run `watch -n1 cat /proc/interrupts` during a large file
  copy to watch the storage device's completion interrupts climb.

## Related

- [Reading and writing from and to memory](reading-n-writing-from-n-to-memory.md) - the CPU's
  path to memory; DMA is the device's path.
- [Persistent storage](<../storage management/persistent-storage.md>) - NVMe queues in more detail.
- [Asynchronous I/O](<../socket management/asynchronous-io.md>) - from the program's point of
  view, every DMA transfer is already asynchronous; async APIs expose that.
