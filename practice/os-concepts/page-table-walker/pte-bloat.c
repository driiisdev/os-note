// why process-per-connection servers (like Postgres) can burn memory on PAGE TABLES:
// one 512MB shared memory region (think shared_buffers), N worker processes that all touch
// it. the data exists once - but every process builds its OWN page tables to map it.
//   ./pte-bloat [workers=8] [shared-MB=512]
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static long status_kb(pid_t pid, const char *key) {
    char path[64], line[256];
    long kb = 0;
    snprintf(path, sizeof path, "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    while (f && fgets(line, sizeof line, f))
        if (strncmp(line, key, strlen(key)) == 0) sscanf(line + strlen(key) + 1, "%ld", &kb);
    if (f) fclose(f);
    return kb;
}

int main(int argc, char **argv) {
    int workers = argc > 1 ? atoi(argv[1]) : 8;
    size_t size = (size_t)(argc > 2 ? atoi(argv[2]) : 512) << 20;

    // MAP_SHARED: every forked child sees the same physical pages
    char *shared = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) { perror("mmap"); return 1; }
    memset(shared, 1, size);  // parent populates it once
    printf("shared region: %zu MB, populated once by the parent\n\n", size >> 20);

    int ready[2], done[2];
    if (pipe(ready) < 0 || pipe(done) < 0) return 1;
    pid_t pids[256];
    for (int i = 0; i < workers && i < 256; i++) {
        if ((pids[i] = fork()) == 0) {
            close(done[1]);  // our inherited copy of the write end would stop EOF ever arriving
            volatile char sink = 0;
            for (size_t off = 0; off < size; off += 4096) sink += shared[off];  // touch every page
            (void)sink;
            if (write(ready[1], "r", 1) != 1) _exit(1);  // tell the parent we're mapped
            char c;
            if (read(done[0], &c, 1) < 0) _exit(1);      // stay alive so we can be measured
            _exit(0);
        }
    }
    for (int i = 0; i < workers; i++) { char c; if (read(ready[0], &c, 1) != 1) return 1; }

    long total_pte = 0;
    printf("  %-8s %10s %14s\n", "worker", "VmPTE", "RssShmem");
    for (int i = 0; i < workers; i++) {
        long pte = status_kb(pids[i], "VmPTE");
        total_pte += pte;
        printf("  %-8d %7ld KB %11ld KB\n", pids[i], pte, status_kb(pids[i], "RssShmem"));
    }
    printf("\n  %d workers x ~%ld KB = %ld KB of page tables, all describing the SAME %zu MB.\n",
           workers, total_pte / workers, total_pte, size >> 20);
    printf("  with 2MB huge pages each worker would need ~1/512 of that.\n");

    close(done[1]);  // EOF wakes every child
    for (int i = 0; i < workers; i++) waitpid(pids[i], NULL, 0);
    return 0;
}
