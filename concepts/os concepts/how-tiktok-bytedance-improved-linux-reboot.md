# How TikTok's ByteDance Improved Linux Reboot

In July 2022, a ByteDance engineer posted a patch series to the Linux kexec mailing list titled
**"faster kexec reboot"**. It targeted a few hundred milliseconds in the kexec reboot path that
nobody had worried about - because for most machines, a reboot takes minutes anyway. For a company
that reboots huge fleets of servers and lightweight VMs into new kernels, those milliseconds are
downtime multiplied by every machine.

*Scope note: the details below come from the patch cover letter, the review discussion and news
coverage of it (sources at the end). Whether the lecture covers exactly this series, and how much
of it was eventually merged upstream, is worth checking against your own notes and the kernel git
history - the review raised real objections.*

## Background: what kexec is

A normal reboot goes **kernel → firmware (BIOS/UEFI, POST, device init) → bootloader → kernel**.
On big servers, the firmware part alone can take minutes.

**kexec** skips it: the running kernel loads a new kernel image into memory (`kexec -l`), then
jumps directly into it (`kexec -e` / `systemctl kexec`). It's how operators upgrade host kernels
quickly, and it's the mechanism behind **kdump** (on a crash, jump into a small pre-loaded "crash
kernel" that saves the dead kernel's memory to disk).

```text
 normal reboot:  [old kernel shutdown] -> [firmware / POST ........] -> [bootloader] -> [new kernel]
 kexec reboot:   [old kernel shutdown] -> [machine_kexec -> purgatory -> new image] -> [new kernel]
                                            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                                            the part this patch series measured
```

## Where the time was going

Between `machine_kexec()` in the old kernel and `start_kernel()` in the new one, the cover letter
measured (with `rdtsc` timestamps) **over 500ms** spent on three steps:

| Step | What happens | Why it's slow |
| --- | --- | --- |
| **Copy** | the new image is moved from wherever `kexec -l` staged it to its final physical address | tens of MB of `memcpy` in a minimal environment |
| **Verify** | **purgatory** - a tiny standalone program that runs between the two kernels - checks a SHA-256 digest of the loaded segments, computed at load time, to detect memory corruption | SHA-256 over the whole image, in simple unoptimized code |
| **Decompress** | a compressed kernel (bzImage) runs its built-in decompressor to unpack the real kernel | decompression of tens of MB |

🖼️ **the kexec path:**
![kexec reboot path](<../../assets/img/kexec-reboot.png>)

## The changes proposed

1. **Reuse the crash-kernel reserved memory for normal kexec.** That region (`crashkernel=` on the
   kernel command line) is already reserved and never used by the running system, so the new
   kernel can be loaded *directly where it will run* - nothing left to copy at reboot time.
2. **A build-time option to skip purgatory's SHA-256 check.**
3. **Support booting an uncompressed kernel on x86**, and skip the copy for it. Coverage of the
   series reported this alone saved **over 150ms** - at the cost of the image growing from
   **8.5MB to 53MB**.

Headline result in the cover letter: `machine_kexec` → `start_kernel` in **~15ms**.

## The pushback

The kexec maintainer, Eric Biederman, questioned several points in review:

- **Driver shutdown** - how were devices being shut down and re-initialized? Those steps would
  typically matter more than memory handling, and skipping them carelessly risks hardware left in
  a bad state.
- **Skipping the SHA-256 check** gives up protection against memory corruption between load and
  boot.
- **The test setup** - whether the timings came from a small VM rather than a real server.

The author's answer was the use case: some lightweight VMs boot their kernel in ~100-200ms, so a
500ms kexec overhead is several times the boot itself.

That exchange is the real lesson of the story: every one of these steps exists for a reason
(integrity checking, small images, flexibility about where an image is loaded), and an
optimization means deciding which guarantee you're willing to give up for a particular workload.

## The OS concepts underneath

- **Boot is a pipeline of stages**, and once the slowest stage (firmware) is removed by kexec, the
  next-slowest ones become worth optimizing - the same reasoning as Amdahl's law
  ([pipelining and parallelism](<../inside the cpu/pipelining-n-parallelism.md>)).
- **Memory bandwidth and simple loops** - copying and hashing tens of MB is cheap in a warmed-up
  user process, but not in purgatory's minimal environment.
- **Size vs speed trade-offs** - compression saves disk and memory but costs CPU on every boot;
  the right choice depends on what's scarce.
- **Reserved memory** - the crashkernel region is memory the OS deliberately never uses, kept free
  for a rainy day; reusing it is clever precisely because it's guaranteed to be untouched.

## Practice project

[`practice/os-concepts/kexec-reboot-sim`](../../practice/os-concepts/kexec-reboot-sim) - times
copy / SHA-256 / decompress on a synthetic 53MB kernel for each configuration: the classic path
(~320ms with gzip, ~900ms with xz in user space), the uncompressed image (no decompression, but a
6x bigger copy and hash), and the fully patched path (nothing left to do).

## Sources

- [PATCH 0/4] faster kexec reboot - [kexec mailing list](https://lists.infradead.org/pipermail/kexec/2022-July/025487.html),
  [review discussion](https://lists.infradead.org/pipermail/kexec/2022-July/025504.html),
  [LKML thread](https://lore.kernel.org/all/Yt6GPsWcG9iWHBUg@zx2c4.com/T/)
- [Phoronix: ByteDance Working To Make It Faster Kexec Booting The Linux Kernel](https://www.phoronix.com/news/Bytedance-Faster-Kexec-Reboot)
- [LWN coverage](https://lwn.net/Articles/902401/)

## Related

- [Compilers and linkers](compilers-n-linkers.md) - what's inside the kernel image being loaded.
- [Virtualization and containerization](virtualization-n-containerization.md) - the lightweight VMs
  that motivated the work.
- [Page tables](page-tables.md) - part of what the new kernel has to build from scratch on boot.
