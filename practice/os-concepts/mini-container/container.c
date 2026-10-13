// a container in ~100 lines: no Docker, just the kernel features Docker is built on.
//   namespaces  -> what the process can SEE   (its own hostname, PID 1, mount table)
//   chroot      -> which directory tree is its "/"
//   cgroups     -> how much it can USE         (a memory limit; v2, or v1 as on stock WSL2)
//
//   sudo ./bin/container <rootfs-dir> [memory-limit-MB] [command...]
//   sudo ./bin/container ./rootfs 64 /bin/sh
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// cgroup v2 has one unified tree; v1 has one tree per controller with different file names
#define CG2      "/sys/fs/cgroup/mini-container"
#define CG1_MEM  "/sys/fs/cgroup/memory/mini-container"

static const char *cgroup_dir;
static char *rootfs;
static char **cmd;
static char child_stack[1024 * 1024];
static int sync_pipe[2];  // the child waits on this until the host has finished setting it up

static void die(const char *what) { perror(what); exit(1); }

static void write_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0 || write(fd, value, strlen(value)) < 0) {
        fprintf(stderr, "warning: could not write '%s' to %s: %s\n", value, path, strerror(errno));
    }
    if (fd >= 0) close(fd);
}

// runs INSIDE the new namespaces
static int child(void *arg) {
    (void)arg;
    // block until the host has moved us into the cgroup - otherwise we could start running
    // (and allocating) before the limit applies. real runtimes like runc do the same handshake
    char go;
    close(sync_pipe[1]);
    if (read(sync_pipe[0], &go, 1) != 1) die("sync");
    close(sync_pipe[0]);

    // UTS namespace: this hostname change is invisible to the host
    if (sethostname("container", strlen("container")) < 0) die("sethostname");

    // mount namespace: stop our mounts from propagating back to the host's mount table
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) die("mount private");

    // filesystem isolation: make rootfs our "/"
    if (chroot(rootfs) < 0) die("chroot");
    if (chdir("/") < 0) die("chdir");

    // PID namespace: a fresh /proc so tools like ps only see processes in OUR namespace
    mkdir("/proc", 0555);
    if (mount("proc", "/proc", "proc", 0, NULL) < 0) perror("mount /proc");

    printf("[container] I am pid %d, hostname 'container', root is %s on the host\n",
           getpid(), rootfs);
    fflush(stdout);
    execvp(cmd[0], cmd);
    die("exec");
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: sudo %s <rootfs> [mem-MB] [cmd...]\n", argv[0]);
        return 1;
    }
    rootfs = argv[1];
    int mem_mb = argc > 2 ? atoi(argv[2]) : 0;
    static char *default_cmd[] = { "/bin/sh", NULL };
    cmd = argc > 3 ? &argv[3] : default_cmd;

    // cgroups: create a group, cap its memory, and (after clone) put the container in it.
    // a cgroup is just a directory - the kernel fills it with control files on mkdir()
    if (mem_mb > 0) {
        char limit[32];
        int v2 = access("/sys/fs/cgroup/cgroup.controllers", F_OK) == 0;
        cgroup_dir = v2 ? CG2 : CG1_MEM;
        mkdir(cgroup_dir, 0755);
        if (v2) {
            snprintf(limit, sizeof limit, "%dM", mem_mb);
            write_file(CG2 "/memory.max", limit);
            write_file(CG2 "/memory.swap.max", "0");
        } else {
            snprintf(limit, sizeof limit, "%ld", (long)mem_mb << 20);
            write_file(CG1_MEM "/memory.limit_in_bytes", limit);
            write_file(CG1_MEM "/memory.swappiness", "0");  // hit the limit instead of swapping
        }
        printf("[host] cgroup %s (%s): memory limit %d MB\n", cgroup_dir, v2 ? "v2" : "v1", mem_mb);
    }

    // clone() is fork() with options: each CLONE_NEW* flag gives the child a new namespace
    if (pipe(sync_pipe) < 0) die("pipe");
    int flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD;
    pid_t pid = clone(child, child_stack + sizeof child_stack, flags, NULL);
    if (pid < 0) die("clone (are you root?)");
    printf("[host] started container: host sees it as pid %d\n", pid);

    if (mem_mb > 0) {
        char pidstr[16], procs[128];
        snprintf(pidstr, sizeof pidstr, "%d", pid);
        snprintf(procs, sizeof procs, "%s/cgroup.procs", cgroup_dir);
        write_file(procs, pidstr);  // from now on, the limit applies (and to every child it forks)
    }
    fflush(stdout);
    close(sync_pipe[0]);
    if (write(sync_pipe[1], "g", 1) != 1) die("sync");  // setup done: let the child run
    close(sync_pipe[1]);

    int status;
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status))
        printf("[host] container killed by signal %d%s\n", WTERMSIG(status),
               WTERMSIG(status) == SIGKILL ? " (the OOM killer enforcing the memory limit?)" : "");
    else
        printf("[host] container exited with %d\n", WEXITSTATUS(status));
    if (mem_mb > 0) rmdir(cgroup_dir);  // only works once the group has no processes left
    return 0;
}
