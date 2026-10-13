#!/usr/bin/env bash
# a tour of the hardware the OS is managing, read straight from what the kernel exposes.
# everything here comes from /proc and /sys - virtual files the kernel generates on demand,
# not real files on disk.

section() { printf '\n== %s ==\n' "$1"; }

section "kernel"
uname -srm

section "cpu (cores, threads, caches)"
if command -v lscpu >/dev/null; then
  lscpu | grep -E '^(Model name|Architecture|CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)|L1d|L1i|L2|L3)'
else
  grep -m1 'model name' /proc/cpuinfo
  echo "logical cpus: $(nproc)"
fi

section "memory"
grep -E '^(MemTotal|MemFree|MemAvailable|Cached|SwapTotal)' /proc/meminfo

section "block devices (storage)"
if command -v lsblk >/dev/null; then
  lsblk -d -o NAME,SIZE,ROTA,TYPE,MODEL 2>/dev/null   # ROTA=1 -> spinning disk, 0 -> SSD
else
  cat /proc/partitions
fi

section "network interfaces"
if command -v ip >/dev/null; then ip -br link; else ls /sys/class/net; fi

section "interrupts (top 8 by total count) - devices tapping the CPU on the shoulder"
# first column is the IRQ, then one count per CPU, then the device name
awk 'NR>1 { s=0; for (i=2; i<=NF; i++) if ($i ~ /^[0-9]+$/) s+=$i; print s, $0 }' /proc/interrupts \
  | sort -rn | head -8 | cut -c1-120
