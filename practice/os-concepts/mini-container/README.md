# Practice: Mini Container

Concept note: [`virtualization-n-containerization.md`](<../../../concepts/os concepts/virtualization-n-containerization.md>)

## What this demonstrates

That a "container" is not a thing the Linux kernel knows about. It's an ordinary process
started with a few kernel features turned on - the same ones Docker, containerd and runc use:

| Feature | What it isolates | In `container.c` |
| --- | --- | --- |
| **UTS namespace** | hostname | `CLONE_NEWUTS` + `sethostname("container")` |
| **PID namespace** | process IDs - the container's first process is PID 1 | `CLONE_NEWPID` + a fresh `/proc` |
| **mount namespace** | the mount table | `CLONE_NEWNS` + `MS_PRIVATE` |
| **chroot** | which directory is `/` | `chroot(rootfs)` |
| **cgroup** | how much memory it can use | `memory.max` (v2) / `memory.limit_in_bytes` (v1) |

Unlike a VM, there's no second kernel: the "container" shares the host kernel, and `ps` on the
host can see it like any other process.

## Folder structure

```text
mini-container/
  container.c   <- clone() with namespaces, chroot, /proc, cgroup memory limit
  hog.c         <- allocates 10MB at a time until something stops it
  Makefile      <- `make`, `make rootfs` (downloads Alpine), `make run`
```

## Build & run

Needs root (creating namespaces and cgroups is privileged) and internet access once, for a
~3MB Alpine Linux root filesystem.

```bash
make
make rootfs                       # downloads Alpine minirootfs into ./rootfs, builds a static hog
sudo ./bin/container ./rootfs 64 /bin/sh
```

Then, **inside** the container:

```sh
hostname        # container
ps              # only the container's own processes, starting at PID 1
ls /            # Alpine's root, not the host's
cat /etc/os-release
/hog            # allocate until the 64MB limit kills it
exit
```

## What you'll see (expected)

```text
[host] cgroup /sys/fs/cgroup/memory/mini-container (v1): memory limit 64 MB
[host] started container: host sees it as pid 2817
[container] I am pid 1, hostname 'container', root is ./rootfs on the host
/ # hostname
container
/ # ps
PID   USER     TIME  COMMAND
    1 root      0:00 /bin/sh
    2 root      0:00 ps
/ # /hog
hog: 10 MB
hog: 20 MB
...
hog: 60 MB
Killed
/ # exit
[host] container exited with 137
```

(This one wasn't run end-to-end while writing these notes - it needs `sudo` interactively.
The code builds cleanly; the output above is what the kernel features are documented to do.
Your PID and the exact MB where `hog` dies will differ.)

- The same process is **PID 2817 on the host and PID 1 inside** - a PID namespace is a
  translation table, not a separate machine.
- `hog` is killed by the kernel's **OOM killer**, scoped to the cgroup - the host and its
  other processes are unaffected. Exit code 137 = 128 + 9 (SIGKILL).

## Code walkthrough

```c
int flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD;
pid_t pid = clone(child, child_stack + sizeof child_stack, flags, NULL);
```

`clone()` is the system call underneath both `fork()` and `pthread_create()`. The `CLONE_NEW*`
flags each give the child a fresh **namespace** - a private copy of one kind of global kernel
state.

```c
// inside the child
sethostname("container", ...);                  // only visible inside the UTS namespace
mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL); // don't leak our mounts back to the host
chroot(rootfs); chdir("/");                     // Alpine's directory tree becomes "/"
mount("proc", "/proc", "proc", 0, NULL);        // a /proc that reflects OUR pid namespace
execvp(cmd[0], cmd);                            // become /bin/sh - now PID 1 in here
```

```c
// on the host: cgroups are just directories and files
mkdir("/sys/fs/cgroup/memory/mini-container", 0755);
write_file(".../memory.limit_in_bytes", "67108864");
write_file(".../cgroup.procs", "<child pid>");  // the limit now applies to it and its children
```

A **sync pipe** makes the child wait until the host has finished putting it in the cgroup
before it `exec`s anything - the same handshake real runtimes like `runc` perform.

**cgroup v1 vs v2**: stock WSL2 mounts cgroup v1 controllers (`/sys/fs/cgroup/memory/...`);
most modern distros use the unified v2 tree (`/sys/fs/cgroup/<group>/memory.max`). The code
detects which one you have.

## Try this yourself

- From a host terminal while the container runs: `ps -ef | grep sh` (you'll see it with its
  host PID), `ls -l /proc/<host-pid>/ns/` (its namespace IDs differ from `ls -l /proc/self/ns/`),
  and `cat /sys/fs/cgroup/memory/mini-container/memory.usage_in_bytes`.
- Add `CLONE_NEWNET`: the container now has only a `lo` interface, and it's down. Connecting it
  to the host needs a **veth pair** - which is exactly what Docker's bridge network creates.
- Add `CLONE_NEWUSER` and map your UID to 0 inside: a **rootless** container.
- Without writing any C: `sudo unshare --uts --pid --mount --fork --mount-proc chroot rootfs /bin/sh`.
- Replace `chroot` with `pivot_root` - what real runtimes use, because a root user can escape
  a plain `chroot`.

## Real-world equivalent

`docker run --memory=64m --hostname=container alpine sh` does this plus: image layers stacked
with **overlayfs**, network namespaces with veth pairs, user namespaces, **seccomp** syscall
filtering, dropped **capabilities**, and AppArmor/SELinux. On Windows and macOS, Docker Desktop
runs a lightweight Linux **VM** first (on Windows, inside WSL2) - because containers need a
Linux kernel to share.
