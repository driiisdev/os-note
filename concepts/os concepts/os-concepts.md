# OS Concepts - Section Intro

The last section collects concepts that cut across everything before it - how programs are
**built** and **loaded**, how the CPU moves between **user and kernel mode**, how **VMs and
containers** carve one machine into many, how **page tables** really work - plus a set of
real-world case studies where those internals decided the outcome of a production incident or a
major performance win.

## What this section covers

### Foundations

| # | Topic | The question it answers |
| --- | --- | --- |
| 1 | [Compilers and linkers](compilers-n-linkers.md) | How does source code become a running program? |
| 2 | [Kernel vs user mode switching](kernel-vs-user-mode-switching.md) | How does the CPU enforce the OS's authority, and what does crossing the boundary cost? |
| 3 | [Virtualization and containerization](virtualization-n-containerization.md) | VMs vs containers: what's shared, what's isolated, how |
| 4 | [Page tables](page-tables.md) | The data structure behind virtual memory, in full detail |

### Case studies

| # | Topic | The OS concept at its core |
| --- | --- | --- |
| 5 | [How Google improved the Linux TCP/IP stack by 40%](how-google-improved-linux-tcp-ip-stack-by-40-percent.md) | cache lines and struct layout |
| 6 | [How ByteDance improved Linux reboot](how-tiktok-bytedance-improved-linux-reboot.md) | kexec, the boot path, size vs speed trade-offs |
| 7 | [On timeouts](on-timeouts.md) | what the kernel does when the other side stops answering |
| 8 | [Postgres failure caused by a Cisco router (TCP issue)](postgres-failure-caused-by-a-cisco-router-tcp-issue.md) | idle connections, middleboxes, TCP keepalive |

## How the case studies connect back

```text
 Google TCP 40%      -> memory hierarchy, cache lines       (memory management, inside the CPU)
 ByteDance reboot    -> kexec, memory copies, compression   (memory management, OS concepts)
 On timeouts         -> kernel socket state, TCP timers     (socket management)
 Postgres / Cisco    -> idle TCP connections, keepalive     (socket management)
 Page tables         -> virtual memory at scale             (memory management)
```

Each case study is a reminder that the abstractions from earlier sections - "memory is just
memory", "a connection is either open or closed", "a reboot is a reboot" - break down at scale,
and that knowing what's underneath is what lets engineers find and fix the problem.

🖼️ **Image to add - concept map:** a map linking each case study in this section back to the
earlier section(s) it depends on, in the style of the diagram above.
![OS concepts map](<../../assets/img/os-concepts-map.png>)

## Practice projects

| Project | Topic |
| --- | --- |
| [build-pipeline](../../practice/os-concepts/build-pipeline) | compilers and linkers |
| [syscall-cost](../../practice/os-concepts/syscall-cost) | kernel vs user mode |
| [mini-container](../../practice/os-concepts/mini-container) | containerization |
| [page-table-walker](../../practice/os-concepts/page-table-walker) | page tables |
| [struct-cacheline-layout](../../practice/os-concepts/struct-cacheline-layout) | the Google TCP stack case |
| [kexec-reboot-sim](../../practice/os-concepts/kexec-reboot-sim) | the ByteDance reboot case |
| [timeouts-lab](../../practice/os-concepts/timeouts-lab) | timeouts and the Postgres/Cisco case |

## Related

- [OS intro](<../os into/os-intro.md>) - where the course started: why the OS exists.
