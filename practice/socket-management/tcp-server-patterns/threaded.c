// pattern 2: thread per connection. the main thread only accepts; each client gets its own
// thread that can block in read() without holding anyone else up. simple and fast - until
// you have 10,000 clients and therefore 10,000 stacks and a very busy scheduler.
#include <pthread.h>
#include "common.h"

static void *client_thread(void *arg) {
    serve_one((int)(long)arg);
    return NULL;
}

int main(int argc, char **argv) {
    int port = port_arg(argc, argv, 9202);
    int lfd = listen_on(port);
    printf("thread-per-connection server on :%d\n", port);
    for (;;) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) continue;
        pthread_t th;
        pthread_create(&th, NULL, client_thread, (void *)(long)cfd);
        pthread_detach(th);  // nobody will join it - free its resources when it exits
    }
}
