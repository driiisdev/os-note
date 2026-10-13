// demand paging, measured: map 256MB, then touch it one page at a time and count the page
// faults, the RSS growth, and the page-table memory the kernel had to build along the way.
//   ./demand            4KB pages, write to each page  (one fault per 4KB)
//   ./demand huge       ask for 2MB transparent huge pages (one fault per 2MB)
//   ./demand 4k read    only READ each page - the kernel maps the shared zero page instead
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#define SIZE (256UL << 20)

static long minor_faults(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_minflt;
}

// reads one "VmXXX:  1234 kB" line out of /proc/self/status
static long status_kb(const char *key) {
    char line[256];
    long kb = -1;
    FILE *f = fopen("/proc/self/status", "r");
    while (f && fgets(line, sizeof line, f))
        if (strncmp(line, key, strlen(key)) == 0) sscanf(line + strlen(key) + 1, "%ld", &kb);
    if (f) fclose(f);
    return kb;
}

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void report(const char *when) {
    printf("  %-22s RSS %7ld KB   page tables (VmPTE) %5ld KB   minor faults %7ld\n",
           when, status_kb("VmRSS"), status_kb("VmPTE"), minor_faults());
}

int main(int argc, char **argv) {
    int huge = argc > 1 && strcmp(argv[1], "huge") == 0;
    int read_only = argc > 2 && strcmp(argv[2], "read") == 0;

    report("start");
    // reserve 256MB of virtual address space - no physical memory is assigned yet
    volatile char *p = mmap(NULL, SIZE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    madvise((void *)p, SIZE, huge ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
    report("after mmap(256MB)");

    long f0 = minor_faults();
    double t = now_ms();
    long sink = 0;
    for (size_t off = 0; off < SIZE; off += 4096) {
        if (read_only) sink += p[off];
        else p[off] = 1;
    }
    double ms = now_ms() - t;
    long faults = minor_faults() - f0;
    report(read_only ? "after reading each pg" : "after writing each pg");

    printf("\n  %s %s: %ld faults for %lu pages touched, %.1f ms total",
           huge ? "huge pages," : "4KB pages,", read_only ? "read" : "write",
           faults, SIZE / 4096, ms);
    if (faults > 0) printf(" (~%.2f us per fault)", ms * 1000 / faults);
    printf("\n");

    munmap((void *)p, SIZE);
    report("after munmap");
    (void)sink;  // p is volatile, so the reads above happen even though sink is unused
    return 0;
}
