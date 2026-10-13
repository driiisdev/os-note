// watches the heap grow: where malloc gets memory from (brk vs mmap), and the fact that
// "allocated" and "actually backed by RAM" are two different things
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// resident set size: how much of this process is actually in physical RAM right now
static long rss_kb(void) {
    long pages_total, pages_resident;
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f || fscanf(f, "%ld %ld", &pages_total, &pages_resident) != 2) return -1;
    fclose(f);
    return pages_resident * (sysconf(_SC_PAGESIZE) / 1024);
}

int main(void) {
    printf("1) small allocations come from the brk heap - the program break moves up\n");
    void *brk_before = sbrk(0);
    void *small[256];
    for (int i = 0; i < 256; i++) small[i] = malloc(2048);
    void *brk_after = sbrk(0);
    printf("   program break before: %p\n", brk_before);
    printf("   program break after : %p  (+%ld KB for 256 x 2KB mallocs)\n",
           brk_after, ((char *)brk_after - (char *)brk_before) / 1024);
    printf("   (glibc grows the heap in big steps, then carves small chunks out of it)\n\n");

    printf("2) a big allocation skips the heap and gets its own mmap region\n");
    brk_before = sbrk(0);
    char *big = malloc(64 << 20);  // 64MB
    printf("   malloc(64MB) returned %p - program break moved by %ld bytes\n\n",
           (void *)big, (long)((char *)sbrk(0) - (char *)brk_before));

    printf("3) malloc only reserves address space - RAM is committed on first touch\n");
    printf("   RSS after malloc(64MB) : %6ld KB\n", rss_kb());
    memset(big, 1, 32 << 20);      // touch the first half only
    printf("   RSS after touching 32MB: %6ld KB\n", rss_kb());
    memset(big, 1, 64 << 20);
    printf("   RSS after touching 64MB: %6ld KB\n", rss_kb());
    free(big);                     // mmap-backed chunks go straight back to the OS (munmap)
    printf("   RSS after free()       : %6ld KB\n\n", rss_kb());

    printf("4) a leak: memory you can no longer free, still counted against the process\n");
    for (int i = 0; i < 50; i++) {
        char *p = malloc(1 << 20);
        memset(p, 0, 1 << 20);     // touch it so it's real RAM
        p = NULL;                  // pointer lost -> 1MB leaked every iteration
    }
    printf("   RSS after leaking 50 x 1MB: %ld KB (run under valgrind to see it reported)\n",
           rss_kb());

    for (int i = 0; i < 256; i++) free(small[i]);
    return 0;
}
