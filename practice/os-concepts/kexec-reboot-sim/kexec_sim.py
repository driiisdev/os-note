#!/usr/bin/env python3
"""
kexec_sim.py - a user-space model of the slow part of a kexec reboot, and of the three changes
ByteDance's 2022 "faster kexec reboot" patch series proposed for it.

A kexec reboot skips firmware/BIOS: the running kernel jumps straight into a new one. On the way
from machine_kexec() to the new kernel's start_kernel(), three things happen to the new image:

  1. COPY        the image is moved from where `kexec -l` staged it to its final address
  2. VERIFY      "purgatory" (a tiny program between the two kernels) checks a SHA-256 digest
                 of the segments, computed at load time, to catch memory corruption
  3. DECOMPRESS  a compressed kernel (bzImage) unpacks itself into the real kernel

The patches: (a) load the image straight into memory that's already reserved and in place
(the crashkernel region) so there's nothing to copy, (b) a build option to skip the purgatory
SHA-256 check, (c) allow booting an UNCOMPRESSED kernel on x86, trading image size
(8.5MB -> 53MB in their report) for no decompression.

This script times the same three operations on a synthetic 53MB "kernel", in normal user space.
The real ones run in a much more constrained environment (purgatory runs simple, unoptimized
code with the old kernel already torn down), so absolute numbers are NOT the real ones - the
point is which steps exist in each configuration and how their costs compare.

usage: python3 kexec_sim.py [image-MB=53]
"""
import hashlib
import lzma
import os
import random
import sys
import time
import zlib


def synthetic_kernel(size):
    """bytes that compress roughly like a kernel image: mostly repetitive 'code' with some noise"""
    rng = random.Random(42)
    vocab = [os.urandom(8) for _ in range(64)]          # 64 recurring 8-byte "instructions"
    words = rng.choices(vocab, k=size // 8)
    data = bytearray(b"".join(words))
    for _ in range(size // 160):                         # sprinkle in incompressible noise
        i = rng.randrange(0, size - 4)
        data[i:i + 4] = os.urandom(4)
    return bytes(data)


def timed(fn):
    t = time.perf_counter()
    result = fn()
    return result, (time.perf_counter() - t) * 1000


def copy(src):
    dst = bytearray(len(src))
    dst[:] = src                                         # one big memcpy
    return dst


def main():
    size_mb = int(sys.argv[1]) if len(sys.argv) > 1 else 53
    print(f"building a synthetic {size_mb}MB kernel image and compressing it (setup, not timed)...")
    vmlinux = synthetic_kernel(size_mb << 20)
    images = {
        "gzip": zlib.compress(vmlinux, 6),
        "xz": lzma.compress(vmlinux, preset=1),
    }
    for name, blob in images.items():
        print(f"  {name:5} bzImage: {len(blob) / 2**20:5.1f} MB  ({len(vmlinux) / len(blob):.1f}x smaller)")
    print()

    decompress = {"gzip": zlib.decompress, "xz": lzma.decompress}
    rows = []
    for comp in ("gzip", "xz"):
        blob = images[comp]
        _, t_copy = timed(lambda: copy(blob))
        _, t_hash = timed(lambda: hashlib.sha256(blob).digest())
        out, t_dec = timed(lambda: decompress[comp](blob))
        assert out == vmlinux
        rows.append((f"compressed ({comp}), copied, verified", t_copy, t_hash, t_dec))

    _, t_copy = timed(lambda: copy(vmlinux))
    _, t_hash = timed(lambda: hashlib.sha256(vmlinux).digest())
    rows.append(("uncompressed, copied, verified", t_copy, t_hash, 0.0))
    rows.append(("uncompressed, copied, no verify", t_copy, 0.0, 0.0))
    rows.append(("uncompressed, pre-placed, no verify", 0.0, 0.0, 0.0))

    print(f"{'configuration':38} {'copy':>8} {'verify':>8} {'decomp':>8} {'total':>8}   (ms)")
    for name, c, h, d in rows:
        print(f"{name:38} {c:8.1f} {h:8.1f} {d:8.1f} {c + h + d:8.1f}")
    print()
    print("note the trade-off in the middle rows: an uncompressed image removes decompression,")
    ratio = len(vmlinux) / len(images["gzip"])
    print(f"but makes the image {ratio:.0f}x bigger - so copying and hashing it costs MORE than for")
    print("the compressed one. the win only fully arrives when the copy and the check are gone too.")


if __name__ == "__main__":
    main()
