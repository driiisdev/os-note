// shared helpers for the three servers: the socket() -> bind() -> listen() boilerplate,
// and the "protocol":
//   server -> "hi\n"              greeting, sent as soon as the connection is accepted
//   client -> "<some line>\n"     sent only after the client has seen the greeting
//   server -> "echo: <line>\n"    then close
// the greeting matters: without it the kernel would buffer every client's line while the
// server was busy, and even the one-at-a-time server would look fast
#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define GREETING "hi\n"

static inline int listen_on(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);  // allow quick restarts
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port),
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); exit(1); }
    if (listen(fd, 128) < 0) { perror("listen"); exit(1); }
    return fd;
}

static inline int port_arg(int argc, char **argv, int fallback) {
    return argc > 1 ? atoi(argv[1]) : fallback;
}

// blocking version of the protocol, used by the iterative and threaded servers
static inline void serve_one(int cfd) {
    char buf[512], reply[600];
    size_t used = 0;
    ssize_t n;
    if (write(cfd, GREETING, strlen(GREETING)) < 0) { close(cfd); return; }
    // keep reading until a full line has arrived - TCP may deliver it in pieces
    while (used < sizeof buf - 1 && (n = read(cfd, buf + used, sizeof buf - 1 - used)) > 0) {
        used += n;
        if (memchr(buf, '\n', used)) break;
    }
    buf[used] = '\0';
    int len = snprintf(reply, sizeof reply, "echo: %s", buf);
    if (write(cfd, reply, len) < 0) perror("write");
    close(cfd);
}

#endif
