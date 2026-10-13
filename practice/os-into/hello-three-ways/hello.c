// three ways to print "hello" - every one of them ends at the same write(2) system call,
// because user code has no other way to reach the terminal device than asking the kernel
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

// way 3: no libc wrapper at all - load the syscall number and arguments into the registers
// the x86-64 Linux ABI expects, then execute the `syscall` instruction ourselves
static long raw_write(int fd, const void *buf, unsigned long len) {
#if defined(__x86_64__)
    long ret;
    // rax = syscall number, rdi/rsi/rdx = first three args, result comes back in rax.
    // the CPU itself overwrites rcx (return address) and r11 (saved flags) on `syscall`
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"((long)SYS_write), "D"((long)fd), "S"(buf), "d"(len)
                     : "rcx", "r11", "memory");
    return ret;
#else
    // other architectures use a different instruction (svc on ARM, ecall on RISC-V) -
    // fall back to libc's generic syscall() helper so the demo still runs
    return syscall(SYS_write, fd, buf, len);
#endif
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "all";
    int all = strcmp(mode, "all") == 0;

    if (all || strcmp(mode, "printf") == 0) {
        // way 1: stdio formats into a user-space buffer, then calls write() when it flushes
        printf("1) hello via printf()  - libc formats + buffers, then calls write()\n");
        fflush(stdout);
    }
    if (all || strcmp(mode, "write") == 0) {
        // way 2: the thin libc wrapper - sets up registers and executes `syscall` for us
        const char *msg = "2) hello via write()   - thin libc wrapper around the syscall\n";
        if (write(STDOUT_FILENO, msg, strlen(msg)) < 0) return 1;
    }
    if (all || strcmp(mode, "raw") == 0) {
        const char *msg = "3) hello via syscall   - raw instruction, no libc in the path\n";
        long n = raw_write(STDOUT_FILENO, msg, strlen(msg));
        if (n < 0) return 1;
    }
    return 0;
}
