// the page cache on the read side: read a 128MB file "cold" (evicted from the cache) and then
// "warm" (already in RAM), and use mincore() to ask the kernel how much of the file it holds.
// no root needed - posix_fadvise(DONTNEED) evicts this one file's clean pages.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SIZE (128UL << 20)

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

// what percent of the file's pages are currently resident in the page cache?
static double cached_pct(int fd) {
    void *map = mmap(NULL, SIZE, PROT_READ, MAP_SHARED, fd, 0);
    size_t pages = SIZE / 4096, resident = 0;
    unsigned char *vec = malloc(pages);
    mincore(map, SIZE, vec);
    for (size_t i = 0; i < pages; i++) resident += vec[i] & 1;
    free(vec);
    munmap(map, SIZE);
    return 100.0 * resident / pages;
}

static double read_all(int fd) {
    static char buf[1 << 20];
    lseek(fd, 0, SEEK_SET);
    double t = now_ms();
    while (read(fd, buf, sizeof buf) > 0) {}
    return now_ms() - t;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "cachefile.bin";
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return 1; }
    char *chunk = malloc(1 << 20);
    memset(chunk, 'y', 1 << 20);
    for (int i = 0; i < 128; i++)
        if (write(fd, chunk, 1 << 20) != 1 << 20) { perror("write"); return 1; }
    fsync(fd);  // pages must be clean (on disk) before the kernel is allowed to drop them

    printf("just written           : %5.1f%% of the file is in the page cache\n", cached_pct(fd));
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    printf("after FADV_DONTNEED    : %5.1f%% cached\n", cached_pct(fd));

    double cold = read_all(fd);
    printf("cold read (from disk)  : %7.1f ms  %7.0f MB/s   -> now %5.1f%% cached\n",
           cold, 128 / (cold / 1000), cached_pct(fd));
    double warm = read_all(fd);
    printf("warm read (from RAM)   : %7.1f ms  %7.0f MB/s   (%.0fx faster)\n",
           warm, 128 / (warm / 1000), cold / warm);

    close(fd);
    unlink(path);
    free(chunk);
    return 0;
}
