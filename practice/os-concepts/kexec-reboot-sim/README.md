# Practice: kexec Reboot Simulation

Concept note: [`how-tiktok-bytedance-improved-linux-reboot.md`](<../../../concepts/os concepts/how-tiktok-bytedance-improved-linux-reboot.md>)

## What this demonstrates

A **kexec** reboot skips the firmware entirely: the running kernel loads a new kernel into memory
and jumps into it. ByteDance's 2022 *"faster kexec reboot"* patch series measured **over 500ms**
being spent on three steps between `machine_kexec()` and the new kernel's `start_kernel()` -
**copying** the new image into place, **verifying** it (SHA-256, in "purgatory"), and
**decompressing** it - and proposed removing all three, reporting **~15ms** for that stretch.

This script runs the same three operations on a synthetic 53MB "kernel" in user space and times
every combination, so you can see where the time goes and why each patch was needed.

**It's a model, not a reboot.** The real steps run in a constrained environment (purgatory is
simple unoptimized code, running after the old kernel has shut down), so the absolute numbers
here are not the real ones. Which steps exist in each configuration, and how their costs compare,
is the point.

## Folder structure

```text
kexec-reboot-sim/
  kexec_sim.py   <- synthetic kernel image, gzip + xz "bzImage", timed copy / SHA-256 / decompress
```

Python 3 standard library only (`zlib`, `lzma`, `hashlib`) - no build step.

## Build & run

```bash
python3 kexec_sim.py         # 53MB image, like the uncompressed kernel in ByteDance's report
python3 kexec_sim.py 30      # a different image size
```

Setup (building and compressing the image) takes ~10-15 seconds; only the phases in the table are
timed.

## What you'll actually see

```text
building a synthetic 53MB kernel image and compressing it (setup, not timed)...
  gzip  bzImage:  10.8 MB  (4.9x smaller)
  xz    bzImage:   8.8 MB  (6.0x smaller)

configuration                              copy   verify   decomp    total   (ms)
compressed (gzip), copied, verified        24.7     49.6    246.8    321.2
compressed (xz), copied, verified           6.5     28.0    866.4    900.8
uncompressed, copied, verified             40.7    150.7      0.0    191.4
uncompressed, copied, no verify            40.7      0.0      0.0     40.7
uncompressed, pre-placed, no verify         0.0      0.0      0.0      0.0
```

- **Decompression dominates the classic path** - and the compressor matters a lot: xz gives the
  smallest image (8.8MB vs 53MB, the same ~6x as the real 8.5MB → 53MB) but is 3.5x slower to
  unpack than gzip. (That's why the kernel offers LZ4 and zstd as faster-to-decompress options.)
- **An uncompressed image isn't a free win.** It removes decompression, but the image is ~6x
  bigger, so copying and hashing it costs *more* than for the compressed one (the verify column
  triples). ByteDance reported the uncompressed-kernel change alone saved "over 150ms".
- **Only removing all three steps gets near zero** - which is why the series also proposed loading
  the image straight into already-reserved, already-in-place memory (the crashkernel region, so
  there's nothing to copy) and a build option to skip the purgatory SHA-256 check.

## Code walkthrough

```python
def copy(src):
    dst = bytearray(len(src))
    dst[:] = src                         # one big memcpy - "move the image to its final address"

hashlib.sha256(blob).digest()            # purgatory's integrity check of the loaded segments
zlib.decompress(blob)                    # the bzImage decompressor stub unpacking vmlinux
```

The synthetic image is built from a small vocabulary of random 8-byte "instructions" plus scattered
random noise, tuned so it compresses roughly like a real kernel (~5-6x).

## Try this yourself

- `python3 kexec_sim.py 8` vs `python3 kexec_sim.py 100` - every phase scales linearly with image
  size, which is why a smaller (stripped-down) kernel is also a reboot optimization.
- Add a `lz4`-style fast compressor (e.g. `zlib.compress(vmlinux, 1)`) and compare the decompress
  column - compression *level* barely changes decompression speed; the *algorithm* does.
- On a real Linux machine with root (not WSL): `sudo kexec -l /boot/vmlinuz-$(uname -r)
  --initrd=/boot/initrd.img-$(uname -r) --reuse-cmdline && sudo systemctl kexec` reboots into the
  same kernel without going through firmware. Compare the time with a normal reboot. (It will
  restart the machine - only do this on a box you can afford to reboot.)

## Real-world equivalent

- **Hyperscalers live-upgrade host kernels with kexec** to avoid minutes of firmware/POST time on
  big servers; every hundred milliseconds left in the kexec path is downtime for the VMs or
  containers on that host. ByteDance's motivation was exactly that, including lightweight VMs
  whose own kernels start in ~100-200ms - next to which a 500ms kexec overhead is huge.
- **kdump** uses the same machinery: on a crash, the kernel kexecs into a small *crash kernel*
  preloaded in the reserved crashkernel memory, which then saves a dump of the dead kernel's
  memory - the reserved region the patches proposed reusing.
- **Firecracker microVMs** boot uncompressed kernels for the same reason the patches proposed one:
  skipping decompression shaves startup time.
