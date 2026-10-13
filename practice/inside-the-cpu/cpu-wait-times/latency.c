// how long does the CPU wait for one load? pointer-chasing through a random cycle makes every
// load depend on the previous one, so nothing can be prefetched or overlapped - the time per
// step IS the latency of whichever level of the memory hierarchy the working set fits in.
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct node { struct node *next; char pad[56]; };  // one node per 64-byte cache line

static double now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e9 + t.tv_nsec;
}

static double chase(size_t bytes) {
    size_t n = bytes / sizeof(struct node);
    struct node *nodes = malloc(n * sizeof *nodes);
    size_t *order = malloc(n * sizeof *order);
    for (size_t i = 0; i < n; i++) order[i] = i;
    // Sattolo's shuffle: a random permutation that is ONE big cycle through every node
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)rand() % i;
        size_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }
    for (size_t i = 0; i < n; i++) nodes[order[i]].next = &nodes[order[(i + 1) % n]];

    const long steps = 10 * 1000 * 1000;
    struct node *p = &nodes[order[0]];
    for (size_t i = 0; i < n; i++) p = p->next;      // warm up: pull the set into cache
    double t = now_ns();
    for (long i = 0; i < steps; i++) p = p->next;    // each load needs the previous result
    double ns = (now_ns() - t) / steps;

    if (p == NULL) printf("impossible\n");          // keeps p (and the loop) alive
    free(order);
    free(nodes);
    return ns;
}

int main(void) {
    printf("%-10s %10s\n", "working set", "ns / load");
    for (size_t kb = 4; kb <= 256 * 1024; kb *= 2) {
        double ns = chase(kb * 1024);
        if (kb < 1024) printf("%7zu KB %10.2f  ", kb, ns);
        else printf("%7zu MB %10.2f  ", kb / 1024, ns);
        for (int i = 0; i < (int)(ns * 2) && i < 60; i++) putchar('#');
        putchar('\n');
    }
    printf("\nmatch the jumps against your caches: lscpu | grep cache\n");
    return 0;
}
