// how to replace a file so a crash at ANY moment leaves either the old version or the new
// version on disk - never a half-written one. this is what editors, package managers and
// databases do when they rewrite a config or metadata file.
#define _GNU_SOURCE
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int atomic_save(const char *path, const char *data) {
    char tmp[4096], dir[4096];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    snprintf(dir, sizeof dir, "%s", path);

    // 1. write the new content to a temporary file next to the real one
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (write(fd, data, strlen(data)) != (ssize_t)strlen(data)) return -1;
    printf("  1. wrote %zu bytes to %s       (only in the page cache so far)\n", strlen(data), tmp);

    // 2. make the temp file's DATA durable before it becomes visible under the real name
    if (fsync(fd) < 0) return -1;
    close(fd);
    printf("  2. fsync(%s)                   (data is on the device)\n", tmp);

    // 3. rename() atomically swaps which inode the name points to - readers see old or new
    if (rename(tmp, path) < 0) return -1;
    printf("  3. rename(%s -> %s)  (atomic directory update)\n", tmp, path);

    // 4. the rename itself is a change to the DIRECTORY - fsync it so the new name survives
    int dfd = open(dirname(dir), O_RDONLY | O_DIRECTORY);
    if (dfd < 0) return -1;
    fsync(dfd);
    close(dfd);
    printf("  4. fsync(directory)                         (the rename is durable too)\n");
    return 0;
}

int main(void) {
    const char *path = "./app-config.json";
    printf("saving v1:\n");
    atomic_save(path, "{ \"version\": 1 }\n");
    printf("saving v2:\n");
    atomic_save(path, "{ \"version\": 2, \"feature\": true }\n");

    char buf[256] = { 0 };
    int fd = open(path, O_RDONLY);
    if (fd >= 0 && read(fd, buf, sizeof buf - 1) > 0) printf("\nfinal content: %s", buf);
    if (fd >= 0) close(fd);
    unlink(path);
    return 0;
}
