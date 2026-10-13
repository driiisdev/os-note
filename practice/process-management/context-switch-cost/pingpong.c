// measures what a context switch costs: two processes bounce one byte back and forth over a
// pair of pipes. each side blocks in read() until the other writes, so every hop forces the
// scheduler to switch from one process to the other.
//   ./pingpong          both processes pinned to CPU 0 -> real context switches on one core
//   ./pingpong any      let the scheduler spread them over cores -> cross-core wakeups instead
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ROUNDS 100000

static double now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

// voluntary = the task blocked (e.g. in read); nonvoluntary = the scheduler preempted it
static void ctxt_switches(long *vol, long *invol) {
    char line[256];
    FILE *f = fopen("/proc/self/status", "r");
    while (f && fgets(line, sizeof line, f)) {
        sscanf(line, "voluntary_ctxt_switches: %ld", vol);
        sscanf(line, "nonvoluntary_ctxt_switches: %ld", invol);
    }
    if (f) fclose(f);
}

static void pin_to_cpu0(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    sched_setaffinity(0, sizeof set, &set);
}

int main(int argc, char **argv) {
    int pinned = !(argc > 1 && strcmp(argv[1], "any") == 0);
    if (pinned) pin_to_cpu0();  // inherited by the child through fork()

    // baseline: the same write()+read() syscalls with no other process involved -
    // this is the price of the syscalls alone, without any switch
    int self[2];
    if (pipe(self) < 0) return 1;
    char b = 'x';
    double t = now_us();
    for (int i = 0; i < ROUNDS; i++) {
        if (write(self[1], &b, 1) != 1 || read(self[0], &b, 1) != 1) return 1;
    }
    double syscalls_only = (now_us() - t) / ROUNDS;

    int ping[2], pong[2];
    if (pipe(ping) < 0 || pipe(pong) < 0) return 1;
    pid_t pid = fork();
    if (pid == 0) {
        for (int i = 0; i < ROUNDS; i++) {
            if (read(ping[0], &b, 1) != 1 || write(pong[1], &b, 1) != 1) _exit(1);
        }
        _exit(0);
    }

    long v0 = 0, i0 = 0, v1 = 0, i1 = 0;
    ctxt_switches(&v0, &i0);
    t = now_us();
    for (int i = 0; i < ROUNDS; i++) {
        // write wakes the child; our read blocks -> the scheduler switches to the child
        if (write(ping[1], &b, 1) != 1 || read(pong[0], &b, 1) != 1) return 1;
    }
    double round_trip = (now_us() - t) / ROUNDS;
    ctxt_switches(&v1, &i1);
    waitpid(pid, NULL, 0);

    printf("mode: %s\n", pinned ? "both processes pinned to CPU 0" : "free to use any CPU");
    printf("  write+read in one process (no switch): %6.2f us\n", syscalls_only);
    printf("  ping-pong round trip                  : %6.2f us  (2 hops)\n", round_trip);
    printf("  -> each hop costs ~%.2f us on top of the syscalls themselves\n",
           (round_trip - syscalls_only) / 2);
    printf("  this process: %ld voluntary, %ld involuntary context switches during the run\n",
           v1 - v0, i1 - i0);
    return 0;
}
