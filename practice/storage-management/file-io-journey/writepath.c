// what does write() actually wait for? the same 4KB writes, four ways:
//   buffered    write() only copies into the page cache and returns
//   fsync-end   buffered writes, then one fsync() that waits for the disk
//   fsync-each  fsync() after every single write (what "durable on every commit" costs)
//   direct      O_DIRECT: skip the page cache, every write goes to the device
// run it on a real Linux filesystem (e.g. ~ in WSL), not /mnt/c
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHUNK 4096

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

// dirty = modified in the page cache but not yet written to the device
static long dirty_kb(void) {
    char line[256];
    long kb = -1;
    FILE *f = fopen("/proc/meminfo", "r");
    while (f && fgets(line, sizeof line, f)) sscanf(line, "Dirty: %ld kB", &kb);
    if (f) fclose(f);
    return kb;
}

static void run(const char *mode, const char *path, size_t total) {
    int direct = strcmp(mode, "direct") == 0;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | (direct ? O_DIRECT : 0), 0644);
    if (fd < 0) { perror("open (O_DIRECT needs a real Linux filesystem)"); return; }

    // O_DIRECT requires the buffer, offset and length to be aligned to the block size
    void *buf;
    if (posix_memalign(&buf, 4096, CHUNK) != 0) return;
    memset(buf, 'x', CHUNK);

    long dirty_before = dirty_kb();
    double t = now_ms(), returned_at = 0;
    for (size_t done = 0; done < total; done += CHUNK) {
        if (write(fd, buf, CHUNK) != CHUNK) { perror("write"); break; }
        if (strcmp(mode, "fsync-each") == 0) fsync(fd);
    }
    returned_at = now_ms() - t;
    long dirty_after = dirty_kb();
    if (strcmp(mode, "fsync-end") == 0) fsync(fd);
    double ms = now_ms() - t;
    close(fd);
    free(buf);

    printf("  %-11s %4zu MB  write() calls done after %7.1f ms, ", mode, total >> 20, returned_at);
    if (strcmp(mode, "buffered") == 0)
        printf("durable: NOT YET   %8.1f MB/s  (page cache dirty: +%ld KB)\n",
               (total / 1048576.0) / (ms / 1000), dirty_after - dirty_before);
    else
        printf("durable after %7.1f ms  %8.1f MB/s\n", ms, (total / 1048576.0) / (ms / 1000));
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "testfile.bin";
    printf("4KB writes to %s\n", path);
    run("buffered", path, 64 << 20);
    run("fsync-end", path, 64 << 20);
    run("direct", path, 64 << 20);
    run("fsync-each", path, 4 << 20);  // much smaller - this one is slow on purpose
    unlink(path);
    printf("\n'buffered' never waited for the disk - if the machine lost power right after,\n"
           "that data would be gone. only fsync()/O_DIRECT(+O_DSYNC) give you durability.\n");
    return 0;
}
