// four threads each add 1 to a shared counter a million times. the right answer is 4,000,000.
//   ./race none     plain counter++   -> lost updates, wrong answer, different every run
//   ./race mutex    pthread mutex     -> correct, but every increment takes a lock
//   ./race atomic   atomic fetch-add  -> correct, one locked CPU instruction per increment
//   ./race local    per-thread total, combined once at the end -> correct and fastest
#define _POSIX_C_SOURCE 199309L
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define THREADS 4
#define ITERS 1000000

static volatile long counter;  // volatile only stops the compiler caching it in a register -
                               // it does NOT make counter++ atomic
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static const char *mode;

static void *worker(void *arg) {
    (void)arg;
    if (strcmp(mode, "none") == 0) {
        for (int i = 0; i < ITERS; i++)
            counter++;  // load, add, store: another thread can sneak in between them
    } else if (strcmp(mode, "mutex") == 0) {
        for (int i = 0; i < ITERS; i++) {
            pthread_mutex_lock(&lock);    // critical section: one thread at a time
            counter++;
            pthread_mutex_unlock(&lock);
        }
    } else if (strcmp(mode, "atomic") == 0) {
        for (int i = 0; i < ITERS; i++)
            __atomic_fetch_add(&counter, 1, __ATOMIC_RELAXED);  // `lock xadd` on x86
    } else {
        long mine = 0;  // no sharing at all until the very end
        for (int i = 0; i < ITERS; i++) mine++;
        __atomic_fetch_add(&counter, mine, __ATOMIC_RELAXED);
    }
    return NULL;
}

int main(int argc, char **argv) {
    mode = argc > 1 ? argv[1] : "none";
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);

    pthread_t th[THREADS];
    for (int i = 0; i < THREADS; i++) pthread_create(&th[i], NULL, worker, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(th[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &b);
    double ms = (b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6;
    long expected = (long)THREADS * ITERS;
    printf("%-7s counter = %8ld  expected %ld  %-28s %7.1f ms\n", mode, counter, expected,
           counter == expected ? "correct" : "WRONG - updates were lost", ms);
    return 0;
}
