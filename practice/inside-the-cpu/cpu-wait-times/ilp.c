// instruction-level parallelism: a modern core can run several additions at once, but only
// if they don't depend on each other. one accumulator = every add waits for the previous add
// to finish (a data hazard). four accumulators = four independent chains the out-of-order
// core can overlap in its pipelines.
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>

#define N 8192        // 64KB of doubles - stays in L1/L2, so memory isn't the bottleneck
#define PASSES 20000

static double data[N];

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(void) {
    for (int i = 0; i < N; i++) data[i] = i * 0.5;

    double t = now_ms(), one = 0;
    for (int p = 0; p < PASSES; p++)
        for (int i = 0; i < N; i++)
            one += data[i];                 // each add needs the result of the last one
    double t_one = now_ms() - t;

    t = now_ms();
    double a = 0, b = 0, c = 0, d = 0;
    for (int p = 0; p < PASSES; p++)
        for (int i = 0; i < N; i += 4) {
            a += data[i];                   // four chains with no dependency between them
            b += data[i + 1];
            c += data[i + 2];
            d += data[i + 3];
        }
    double four = a + b + c + d;
    double t_four = now_ms() - t;

    printf("1 accumulator : %7.1f ms  (sum %.0f)\n", t_one, one);
    printf("4 accumulators: %7.1f ms  (sum %.0f)\n", t_four, four);
    printf("same number of additions, %.1fx faster when they're independent\n", t_one / t_four);
    return 0;
}
