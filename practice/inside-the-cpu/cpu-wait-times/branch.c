// the same loop over the same numbers, twice: once with the data shuffled, once sorted.
// the work is identical - the only difference is whether the branch predictor can guess
// the `if`. a wrong guess flushes the pipeline, and the CPU waits while it refills.
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N (32 * 1024)
#define PASSES 2000

static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static double run(const int *data, long *out) {
    long sum = 0;
    double t = now_ms();
    for (int pass = 0; pass < PASSES; pass++)
        for (int i = 0; i < N; i++)
            if (data[i] >= 128)    // random data: a coin flip. sorted: false...false, true...true
                sum += data[i];
    *out = sum;
    return now_ms() - t;
}

int main(void) {
    static int data[N];
    srand(42);
    for (int i = 0; i < N; i++) data[i] = rand() % 256;

    long s1, s2;
    double shuffled = run(data, &s1);
    qsort(data, N, sizeof data[0], cmp);
    double sorted = run(data, &s2);

    printf("shuffled data: %7.1f ms   (sum %ld)\n", shuffled, s1);
    printf("sorted data  : %7.1f ms   (sum %ld, identical work)\n", sorted, s2);
    printf("mispredictions made the shuffled run %.1fx slower\n", shuffled / sorted);
    return 0;
}
