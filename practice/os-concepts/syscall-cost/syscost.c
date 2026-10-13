// what does crossing into the kernel cost? time 5 million iterations of:
//   function call       stays in user mode
//   getpid() syscall    user -> kernel -> user, the cheapest real syscall there is
//   clock_gettime()     LOOKS like a syscall, but runs in user mode via the vDSO
//   write() to /dev/null  a syscall that goes through the VFS layer to a (trivial) driver
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define N 5000000

__attribute__((noinline)) static long plain_function(long x) {
    __asm__ volatile("" ::: "memory");  // stop the compiler from deleting the call
    return x + 1;
}

static double now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e9 + t.tv_nsec;
}

int main(void) {
    double t, fn, sys, vdso, wr;
    long sink = 0;

    t = now_ns();
    for (long i = 0; i < N; i++) sink += plain_function(i);
    fn = (now_ns() - t) / N;

    t = now_ns();
    for (long i = 0; i < N; i++) sink += syscall(SYS_getpid);  // raw: glibc can't cache it
    sys = (now_ns() - t) / N;

    struct timespec ts;
    t = now_ns();
    for (long i = 0; i < N; i++) { clock_gettime(CLOCK_MONOTONIC, &ts); sink += ts.tv_nsec; }
    vdso = (now_ns() - t) / N;

    int fd = open("/dev/null", O_WRONLY);
    char c = 'x';
    t = now_ns();
    for (long i = 0; i < N; i++) sink += write(fd, &c, 1);
    wr = (now_ns() - t) / N;

    printf("  %-28s %8.1f ns\n", "plain function call", fn);
    printf("  %-28s %8.1f ns   (%.0fx a function call)\n", "getpid() syscall", sys, sys / fn);
    printf("  %-28s %8.1f ns   (vDSO - no mode switch)\n", "clock_gettime()", vdso);
    printf("  %-28s %8.1f ns\n", "write(/dev/null, 1 byte)", wr);
    printf("\n(sink %ld)\n", sink & 1);
    return 0;
}
