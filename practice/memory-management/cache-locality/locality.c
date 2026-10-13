// two experiments showing that memory is fetched in 64-byte cache lines, not single bytes:
//   1) same data, same sum - row-major vs column-major traversal
//   2) stride sweep - touching every 1st, 2nd, 4th ... int of a big array
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N 4096                  // 4096 x 4096 ints = 64MB, far bigger than any CPU cache
static int grid[N][N];

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(void) {
    // write every element once first, so page faults don't pollute the first timing
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) grid[i][j] = i ^ j;

    printf("1) traversal order over a %d x %d int grid (%d MB)\n", N, N,
           (int)(sizeof grid >> 20));
    long sum = 0;
    double t = now_ms();
    for (int i = 0; i < N; i++)        // row-major: walks memory sequentially,
        for (int j = 0; j < N; j++)    // uses all 16 ints of every cache line it loads
            sum += grid[i][j];
    double row = now_ms() - t;

    t = now_ms();
    for (int j = 0; j < N; j++)        // column-major: jumps 16KB between accesses,
        for (int i = 0; i < N; i++)    // uses 4 bytes of each 64-byte line it loads
            sum += grid[i][j];
    double col = now_ms() - t;
    printf("   row-major   : %7.1f ms\n", row);
    printf("   column-major: %7.1f ms   (%.1fx slower, same %ld-element sum)\n\n",
           col, col / row, (long)N * N);

    // part 2 -------------------------------------------------------------------------------
    size_t len = (64u << 20) / sizeof(int);   // 16M ints = 64MB
    int *a = malloc(len * sizeof(int));
    memset(a, 1, len * sizeof(int));
    printf("2) stride sweep over a 64MB array (a cache line holds 16 ints)\n");
    printf("   %-8s %10s %12s %14s\n", "stride", "accesses", "total ms", "ns / access");
    for (size_t stride = 1; stride <= 256; stride *= 2) {
        t = now_ms();
        for (size_t i = 0; i < len; i += stride) a[i] *= 3;
        double ms = now_ms() - t;
        size_t touches = len / stride;
        printf("   %-8zu %10zu %12.1f %14.2f\n", stride, touches, ms, ms * 1e6 / touches);
    }
    printf("\n   up to stride 16 every cache line still gets loaded, so total time barely\n"
           "   drops even though the work shrinks 16x. past 16, whole lines get skipped.\n");

    printf("\n(checksum %ld - printed so the compiler can't skip the loops)\n", sum + a[len / 2]);
    free(a);
    return 0;
}
