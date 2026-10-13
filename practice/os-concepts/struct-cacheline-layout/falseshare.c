// the other half of the cache-line story: false sharing. two threads, two DIFFERENT
// counters, no shared data at all - but if both counters sit in one 64-byte cache line,
// every write forces that line to bounce between the two cores' caches.
//
// the two threads are pinned to two different PHYSICAL cores: hyperthread siblings share one
// L1 cache, so on them the line has nowhere to bounce and the effect mostly disappears.
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define ITERS 100000000L

struct same_line { volatile long a; volatile long b; };                  // 8 bytes apart
struct own_lines { volatile long a; char pad[56]; volatile long b; };    // 64 bytes apart

static struct same_line shared __attribute__((aligned(64)));
static struct own_lines padded __attribute__((aligned(64)));

struct job { volatile long *counter; int cpu; };

static int core_id(int cpu) {
    char path[96];
    int id = -1;
    snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%d/topology/core_id", cpu);
    FILE *f = fopen(path, "r");
    if (f) { if (fscanf(f, "%d", &id) != 1) id = -1; fclose(f); }
    return id;
}

// the first CPU that sits on a different physical core than CPU 0
static int other_core(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    for (int c = 1; c < n; c++)
        if (core_id(c) != core_id(0)) return c;
    return n > 1 ? 1 : 0;
}

static void *bump(void *p) {
    struct job *j = p;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(j->cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof set, &set);
    for (long i = 0; i < ITERS; i++) (*j->counter)++;
    return NULL;
}

static int cpu_b;

static double race(volatile long *x, volatile long *y) {
    struct timespec a, b;
    pthread_t t1, t2;
    struct job j1 = { x, 0 }, j2 = { y, cpu_b };
    clock_gettime(CLOCK_MONOTONIC, &a);
    pthread_create(&t1, NULL, bump, &j1);
    pthread_create(&t2, NULL, bump, &j2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    clock_gettime(CLOCK_MONOTONIC, &b);
    return (b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6;
}

int main(void) {
    cpu_b = other_core();
    double s = race(&shared.a, &shared.b);
    double p = race(&padded.a, &padded.b);
    printf("2 threads (CPU 0 and CPU %d, different cores) x %ld increments of their OWN counter\n",
           cpu_b, ITERS);
    printf("  counters in the same cache line: %7.1f ms\n", s);
    printf("  counters 64 bytes apart        : %7.1f ms   -> %.1fx faster\n", p, s / p);
    return 0;
}
