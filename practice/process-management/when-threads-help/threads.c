// "when do you use threads?" answered with a measurement: run 32 equal tasks on a pool of
// 1, 2, 4 ... 32 threads, for two kinds of task.
//   ./threads cpu   each task burns ~20ms of pure computation
//   ./threads io    each task sleeps ~20ms, like waiting on a disk or a network reply
// cpu work stops getting faster once threads > cores. io work keeps scaling, because a
// blocked thread isn't using a core at all.
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TASKS 32
#define TASK_MS 20

static int next_task;   // shared work queue: just an index handed out atomically
static int io_mode;
static long spin_per_ms;

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void burn(long iters) {
    volatile unsigned long x = 1;
    for (long i = 0; i < iters; i++) x = x * 2862933555777941757UL + 3037000493UL;
}

static void *worker(void *arg) {
    (void)arg;
    while (__atomic_fetch_add(&next_task, 1, __ATOMIC_RELAXED) < TASKS) {
        if (io_mode) {
            struct timespec d = { 0, TASK_MS * 1000000L };
            nanosleep(&d, NULL);  // the thread blocks; the core is free for someone else
        } else {
            burn(spin_per_ms * TASK_MS);  // the thread needs a core the whole time
        }
    }
    return NULL;
}

static double run(int threads) {
    next_task = 0;
    pthread_t th[TASKS];
    double t = now_ms();
    for (int i = 0; i < threads; i++) pthread_create(&th[i], NULL, worker, NULL);
    for (int i = 0; i < threads; i++) pthread_join(th[i], NULL);
    return now_ms() - t;
}

int main(int argc, char **argv) {
    io_mode = argc > 1 && strcmp(argv[1], "io") == 0;

    // calibrate: how many loop iterations make one millisecond of work on this machine?
    double t = now_ms();
    burn(10 * 1000 * 1000);
    spin_per_ms = (long)(10 * 1000 * 1000 / (now_ms() - t));

    printf("%d %s tasks of ~%dms each, %ld CPU cores online\n", TASKS,
           io_mode ? "I/O-bound (sleeping)" : "CPU-bound", TASK_MS, sysconf(_SC_NPROCESSORS_ONLN));
    printf("  %-8s %10s %10s\n", "threads", "total ms", "speedup");
    double base = 0;
    for (int n = 1; n <= TASKS; n *= 2) {
        double ms = run(n);
        if (n == 1) base = ms;
        printf("  %-8d %10.0f %9.1fx\n", n, ms, base / ms);
    }
    return 0;
}
