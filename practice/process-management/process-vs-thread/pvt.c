// process vs thread, measured three ways:
//   1) memory: a child process gets a COPY of the address space, a thread shares the original
//   2) identity: threads share the process id but each has its own thread id
//   3) cost: creating processes vs threads - and why fork() gets slower as the parent grows
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int counter = 0;  // one global, in .data

static double now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

static void *increment(void *arg) {
    (void)arg;
    counter += 1;
    printf("   thread : pid %d, tid %d - incremented counter to %d\n", getpid(), gettid(), counter);
    return NULL;
}

static void *nothing(void *arg) { return arg; }

// average cost of fork() + child exit + wait, in microseconds
static double fork_cost(int n) {
    double t = now_us();
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) _exit(0);
        waitpid(pid, NULL, 0);
    }
    return (now_us() - t) / n;
}

int main(void) {
    printf("1) who sees the write?\n");
    printf("   main   : pid %d, tid %d - counter = %d\n", getpid(), gettid(), counter);
    fflush(stdout);

    pid_t pid = fork();
    if (pid == 0) {
        counter += 1;
        printf("   child  : pid %d, tid %d - incremented counter to %d\n", getpid(), gettid(), counter);
        fflush(stdout);
        _exit(0);
    }
    waitpid(pid, NULL, 0);
    printf("   main   : after the child process  -> counter = %d (child changed its own copy)\n", counter);

    pthread_t th;
    pthread_create(&th, NULL, increment, NULL);
    pthread_join(th, NULL);
    printf("   main   : after the thread         -> counter = %d (thread changed OUR memory)\n\n", counter);

    const int n = 2000;
    printf("2) creation cost, averaged over %d create+exit+wait cycles\n", n);
    double t = now_us();
    for (int i = 0; i < n; i++) {
        pthread_create(&th, NULL, nothing, NULL);
        pthread_join(th, NULL);
    }
    double thread_us = (now_us() - t) / n;
    double proc_us = fork_cost(n);
    printf("   thread : %6.1f us\n", thread_us);
    printf("   process: %6.1f us   (%.1fx a thread)\n\n", proc_us, proc_us / thread_us);

    printf("3) fork() copies the parent's page tables - so it costs more as the parent grows\n");
    size_t sizes_mb[] = { 0, 256, 1024 };
    char *big = NULL;
    for (int i = 0; i < 3; i++) {
        if (sizes_mb[i]) {
            big = realloc(big, sizes_mb[i] << 20);
            memset(big, 1, sizes_mb[i] << 20);  // touch it so every page is really mapped
        }
        printf("   parent with %4zu MB touched: fork ~%7.1f us\n", sizes_mb[i], fork_cost(50));
    }
    free(big);
    return 0;
}
