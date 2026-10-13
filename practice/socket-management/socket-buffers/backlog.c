// the accept queue, made visible: a server calls listen(fd, 2) and then never calls accept().
// eight clients try to connect. the kernel completes handshakes on the app's behalf until the
// queue is full - after that, new SYNs are dropped and those clients just hang and retry.
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define CLIENTS 8
#define BACKLOG 2

int main(void) {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(9100),
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    listen(lfd, BACKLOG);
    printf("server: listen(fd, %d) on 127.0.0.1:9100 - and never calls accept()\n\n", BACKLOG);

    int fds[CLIENTS];
    for (int i = 0; i < CLIENTS; i++) {
        fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        fcntl(fds[i], F_SETFL, O_NONBLOCK);
        connect(fds[i], (struct sockaddr *)&addr, sizeof addr);  // returns EINPROGRESS
    }

    // a non-blocking connect is finished when the socket becomes writable
    for (int i = 0; i < CLIENTS; i++) {
        struct pollfd p = { .fd = fds[i], .events = POLLOUT };
        int ready = poll(&p, 1, 1500);
        int err = 0;
        socklen_t len = sizeof err;
        getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &err, &len);
        if (ready > 0 && err == 0)
            printf("client %d: connected      (handshake done, sitting in the accept queue)\n", i + 1);
        else
            printf("client %d: still waiting  (SYN dropped - accept queue full, kernel will retry)\n", i + 1);
    }

    printf("\nthe kernel's view (Recv-Q = connections queued, Send-Q = queue limit):\n");
    fflush(stdout);
    if (system("ss -ltn 'sport = :9100'") != 0) printf("(install iproute2 for ss)\n");

    printf("\nserver: now calling accept() in a loop...\n");
    fcntl(lfd, F_SETFL, O_NONBLOCK);
    int accepted = 0, cfd;
    while ((cfd = accept(lfd, NULL, NULL)) >= 0) { accepted++; close(cfd); }
    printf("server: accept() returned %d connections, then EAGAIN (queue empty)\n", accepted);

    for (int i = 0; i < CLIENTS; i++) close(fds[i]);
    close(lfd);
    return 0;
}
