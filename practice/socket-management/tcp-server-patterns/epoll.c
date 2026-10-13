// pattern 3: one thread, non-blocking sockets, an epoll event loop (the reactor pattern -
// what nginx, redis, and node's libuv do). the thread never waits on any single client: it
// asks the kernel "which of my sockets are ready?" and only touches those.
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "common.h"

#define MAX_FDS 4096
static char bufs[MAX_FDS][512];  // per-connection state: the bytes received so far
static size_t used[MAX_FDS];

static void set_nonblocking(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK); }

int main(int argc, char **argv) {
    int port = port_arg(argc, argv, 9203);
    int lfd = listen_on(port);
    set_nonblocking(lfd);

    int ep = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = lfd };
    epoll_ctl(ep, EPOLL_CTL_ADD, lfd, &ev);
    printf("epoll event-loop server on :%d\n", port);

    struct epoll_event ready[64];
    for (;;) {
        int n = epoll_wait(ep, ready, 64, -1);  // the ONLY place this thread ever blocks
        for (int i = 0; i < n; i++) {
            int fd = ready[i].data.fd;

            if (fd == lfd) {
                // the listening socket is "readable" = connections are waiting in the queue
                int cfd;
                while ((cfd = accept(lfd, NULL, NULL)) >= 0) {
                    if (cfd >= MAX_FDS) { close(cfd); continue; }
                    set_nonblocking(cfd);
                    used[cfd] = 0;
                    // 3 bytes into an empty send buffer - a non-blocking write always fits
                    if (write(cfd, GREETING, strlen(GREETING)) < 0) { close(cfd); continue; }
                    struct epoll_event cev = { .events = EPOLLIN, .data.fd = cfd };
                    epoll_ctl(ep, EPOLL_CTL_ADD, cfd, &cev);
                }
                continue;
            }

            // a client socket is readable: take whatever is there, never wait for more
            ssize_t r = read(fd, bufs[fd] + used[fd], sizeof bufs[fd] - 1 - used[fd]);
            if (r < 0 && errno == EAGAIN) continue;
            if (r > 0) used[fd] += r;
            int line_done = memchr(bufs[fd], '\n', used[fd]) != NULL;
            if (r > 0 && !line_done && used[fd] < sizeof bufs[fd] - 1) continue;  // partial line

            if (r > 0) {
                // small reply, so a single write() fits in the send buffer. a real server would
                // also watch EPOLLOUT and keep unsent bytes around for the next readiness event
                char reply[600];
                bufs[fd][used[fd]] = '\0';
                int len = snprintf(reply, sizeof reply, "echo: %s", bufs[fd]);
                if (write(fd, reply, len) < 0) perror("write");
            }
            epoll_ctl(ep, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
        }
    }
}
