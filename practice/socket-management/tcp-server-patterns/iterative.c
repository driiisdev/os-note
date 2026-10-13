// pattern 1: one thread, blocking calls, one client at a time.
// while serve_one() is stuck in read() waiting for a slow client, nobody calls accept() -
// every other client waits in the kernel's accept queue.
#include "common.h"

int main(int argc, char **argv) {
    int port = port_arg(argc, argv, 9201);
    int lfd = listen_on(port);
    printf("iterative server on :%d\n", port);
    for (;;) {
        int cfd = accept(lfd, NULL, NULL);  // blocks until a connection is queued
        if (cfd < 0) continue;
        serve_one(cfd);                     // blocks until THIS client is done
    }
}
