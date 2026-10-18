# Virtualization and Containerization

Both let one physical machine run many isolated workloads, but they isolate at different layers.
A **virtual machine** emulates a whole computer, so each VM runs its **own kernel**. A
**container** is a group of ordinary processes on a **shared kernel**, which the kernel shows a
restricted, private view of the system.

```text
        virtual machines                                containers
 +-----------+ +-----------+ +-----------+     +-----------+ +-----------+ +----------+
 |   app     | |   app     | |   app     |     |    app    | |    app    | |   app    |
 | libs/bins | | libs/bins | | libs/bins |     | libs/bins | | libs/bins | |libs/bins |
 | guest OS  | | guest OS  | | guest OS  |     +-----------+ +-----------+ +----------+
 |  KERNEL   | |  KERNEL   | |  KERNEL   |     |  container runtime (containerd/runc) |
 +-----------+ +-----------+ +-----------+     +--------------------------------------+
 |           hypervisor                  |     |       ONE shared host kernel         |
 +---------------------------------------+     |  (namespaces + cgroups + seccomp)    |
 |      hardware (VT-x / AMD-V)          |     +--------------------------------------+
 +---------------------------------------+     |              hardware                |
                                               +--------------------------------------+
```

🖼️ **VMs vs containers:**
![VMs vs containers](<../../assets/img/vm-vs-container.png>)

## Virtual machines

A **hypervisor** (virtual machine monitor) runs guest operating systems that believe they own the
hardware.

| Type | Runs on | Examples |
| --- | --- | --- |
| **Type 1** (bare metal) | directly on the hardware | VMware ESXi, Xen, Microsoft Hyper-V, KVM (the Linux kernel itself becomes the hypervisor) |
| **Type 2** (hosted) | as an application on a host OS | VirtualBox, VMware Workstation, Parallels |

How guests are kept in line:

- **Hardware-assisted virtualization** (Intel VT-x, AMD-V) - a new CPU mode in which the guest
  kernel runs in its own "ring 0", but privileged operations (touching real devices, changing
  certain control registers) cause a **VM exit** into the hypervisor, which emulates them.
- **Nested / extended page tables** (EPT, NPT) - two levels of address translation: guest virtual
  → guest physical (the guest's page tables) → host physical (the hypervisor's). The MMU walks
  both, which is why TLB misses cost more in VMs and huge pages matter even more.
- **Paravirtualized devices** (virtio) - instead of emulating a real network card register by
  register, the guest uses a driver designed for virtualization that talks to the hypervisor
  through shared-memory queues - much faster.
- **IOMMU passthrough** - give a VM a real device directly (GPUs, NICs) with DMA safely confined.

**WSL2 is a lightweight Hyper-V VM** running a real Linux kernel - which is why the practice
projects in this repo work there, and why some measurements show hypervisor overhead.

## Containers

There's no "container" object in the Linux kernel. A container is a process tree started with
three kinds of kernel features:

### Namespaces - what a process can *see*

| Namespace | Isolates |
| --- | --- |
| `pid` | process IDs - the container's first process is PID 1, and it can't see host processes |
| `mnt` | the mount table - its own root filesystem |
| `net` | network interfaces, IPs, routing tables, ports |
| `uts` | hostname and domain name |
| `ipc` | System V IPC, POSIX message queues |
| `user` | user and group IDs - root inside can map to an unprivileged user outside |
| `cgroup` | the view of the cgroup hierarchy |
| `time` | the boot and monotonic clocks |

### cgroups - what a process can *use*

Control groups limit and account for resources per group of processes: `memory.max`, CPU shares
and quotas (`cpu.max`), I/O bandwidth, the number of processes (`pids.max`). Exceeding the memory
limit invokes the OOM killer *within the group*.

### Filesystem and security layers

- **Image layers** stacked with **overlayfs**: read-only image layers plus one writable layer per
  container.
- **Capabilities** - root's powers split into ~40 pieces; containers drop most of them.
- **seccomp** - a filter on which system calls are allowed at all.
- **AppArmor / SELinux** - mandatory access control profiles.

Docker → containerd → **runc** (the OCI runtime) is the chain that actually calls `clone()` with
`CLONE_NEW*` flags, sets up cgroups and mounts, and `exec`s your entrypoint.

## Comparison

| | Virtual machine | Container |
| --- | --- | --- |
| Isolation boundary | hardware virtualization - strong | kernel features - weaker (one shared kernel; a kernel bug can be an escape) |
| Kernel | one per VM, any OS (Windows guest on Linux host is fine) | the host's - Linux containers need a Linux kernel |
| Startup | seconds to minutes (boot an OS) | milliseconds (start a process) |
| Overhead | memory for each guest OS, some CPU/I/O virtualization cost | close to native |
| Density | tens per host | hundreds to thousands per host |
| Typical use | multi-tenant clouds, different OSes, strong isolation | packaging and deploying apps, microservices, CI |

**In between**: **microVMs** (AWS Firecracker - behind Lambda and Fargate) boot a minimal VM in
~100ms to get VM isolation at container-like speed; **gVisor** intercepts a container's syscalls
with a user-space kernel; **Kata Containers** run each container in its own lightweight VM.

## Clarifications

- **Containers are not lightweight VMs.** There's no guest kernel; `ps` on the host shows every
  container's processes.
- **Docker on Windows/macOS runs a Linux VM** (WSL2 or a hidden VM) because Linux containers need
  a Linux kernel.
- **"It works in a container" doesn't mean the kernel is identical** - the container brings its own
  userland but uses whatever kernel version the host runs.

## Practice project

[`practice/os-concepts/mini-container`](../../practice/os-concepts/mini-container) - a container in
~130 lines of C: `clone()` with UTS/PID/mount namespaces, `chroot` into an Alpine root filesystem,
a fresh `/proc`, a cgroup memory limit (v1 or v2), and a `hog` program that gets OOM-killed inside
the container without affecting the host. Needs `sudo`.

## Related

- [Kernel vs user mode switching](kernel-vs-user-mode-switching.md) - the hypervisor adds a level
  below the kernel.
- [Page tables](page-tables.md) - nested paging in VMs.
- [Process vs thread](<../process management/process-vs-thread.md>) - `clone()` flags, which
  namespaces are just more of.
