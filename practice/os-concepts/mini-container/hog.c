// allocates (and touches) memory 10MB at a time until something stops it.
// run inside the container with a memory limit to watch the cgroup OOM killer step in.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    for (int mb = 10;; mb += 10) {
        char *p = malloc(10 << 20);
        if (!p) { printf("malloc failed at %d MB\n", mb); return 1; }
        memset(p, 1, 10 << 20);  // touching makes it real memory, charged to our cgroup
        printf("hog: %d MB\n", mb);
        fflush(stdout);
        usleep(100 * 1000);
    }
}
