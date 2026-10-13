// the idea behind Google's Linux networking speedup, in miniature: the SAME struct, the SAME
// fields, the SAME loop - only the ORDER of the fields changes. if the fields the hot path
// reads are spread across 4 cache lines, every struct costs 4 cache lines of space and 4
// fetches; grouped together, it costs 1.
//
// structs are visited in random order (like packets arriving for random sockets), so the
// hardware prefetcher can't hide the difference.
#define _POSIX_C_SOURCE 200112L
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// hot fields interleaved with cold ones - the way structs grow organically over years
struct scattered {
    long rx_bytes;      char stats_a[56];   // line 0
    long rx_packets;    char config_b[56];  // line 1
    long state;         char debug_c[56];   // line 2
    long window;        char misc_d[56];    // line 3
};

// identical contents, hot fields moved into the first cache line
struct grouped {
    long rx_bytes, rx_packets, state, window;            // line 0: everything the loop reads
    char stats_a[56], config_b[56], debug_c[56], misc_d[56];
};

#define ACCESSES (20 * 1000 * 1000)

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

// same loop body for both layouts: read the 4 hot fields of a randomly chosen struct
#define HOT_LOOP(arr)                                                                    \
    for (long i = 0; i < ACCESSES; i++) {                                                \
        long k = order[i % n];                                                           \
        sum += arr[k].rx_bytes + arr[k].rx_packets + arr[k].state + arr[k].window;       \
    }

int main(void) {
    printf("sizeof: scattered %zu B, grouped %zu B (same size, same fields)\n",
           sizeof(struct scattered), sizeof(struct grouped));
    printf("hot field offsets  scattered: %zu %zu %zu %zu   grouped: %zu %zu %zu %zu\n\n",
           offsetof(struct scattered, rx_bytes), offsetof(struct scattered, rx_packets),
           offsetof(struct scattered, state), offsetof(struct scattered, window),
           offsetof(struct grouped, rx_bytes), offsetof(struct grouped, rx_packets),
           offsetof(struct grouped, state), offsetof(struct grouped, window));

    printf("%-9s %13s %13s %13s %13s %8s\n", "structs", "hot data", "hot data",
           "scattered", "grouped", "speedup");
    printf("%-9s %13s %13s %13s %13s\n", "", "(scattered)", "(grouped)", "ms", "ms");
    long sum = 0;
    for (long n = 1024; n <= 256 * 1024; n *= 4) {
        struct scattered *s;
        struct grouped *g;
        long *order = malloc(n * sizeof *order);
        if (posix_memalign((void **)&s, 64, n * sizeof *s) ||
            posix_memalign((void **)&g, 64, n * sizeof *g))
            return 1;
        for (long i = 0; i < n; i++) {
            s[i].rx_bytes = g[i].rx_bytes = i;
            s[i].rx_packets = g[i].rx_packets = s[i].state = g[i].state = 1;
            s[i].window = g[i].window = 2;
            order[i] = i;
        }
        for (long i = n - 1; i > 0; i--) {   // shuffle the visiting order
            long j = rand() % (i + 1), t = order[i];
            order[i] = order[j];
            order[j] = t;
        }

        double t = now_ms();
        HOT_LOOP(s);
        double ts = now_ms() - t;
        t = now_ms();
        HOT_LOOP(g);
        double tg = now_ms() - t;

        printf("%-9ld %10ld KB %10ld KB %13.1f %13.1f %7.1fx\n", n, n * 4 * 64 / 1024,
               n * 64 / 1024, ts, tg, ts / tg);
        free(s);
        free(g);
        free(order);
    }
    printf("\nthe gap is biggest when the grouped hot data fits in a cache level and the\n"
           "scattered version doesn't (compare with: lscpu | grep cache).\n");
    printf("(checksum %ld)\n", sum);
    return 0;
}
