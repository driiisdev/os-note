// a TCP client where every timeout is opt-in, so you can see what happens without each one.
//   ./client [--host IP] [--port N]
//            [--connect-timeout MS]          give up on the handshake after MS
//            [--read-timeout MS]             give up waiting for a reply after MS
//            [--keepalive IDLE,INTVL,COUNT]  probe an idle connection (seconds)
//            [--idle]                        send nothing, just wait (a pooled connection)
// 0 / not given = no timeout = whatever the kernel does by default (often: wait forever)
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static double start;

static double elapsed_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9 - start;
}

static void report(const char *what) {
    printf("  [%6.2fs] %s\n", elapsed_s(), what);
    fflush(stdout);
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = 9300, connect_ms = 0, read_ms = 0, idle = 0;
    int ka_idle = 0, ka_intvl = 0, ka_cnt = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--host") && i + 1 < argc) host = argv[++i];
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--connect-timeout") && i + 1 < argc) connect_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--read-timeout") && i + 1 < argc) read_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keepalive") && i + 1 < argc)
            sscanf(argv[++i], "%d,%d,%d", &ka_idle, &ka_intvl, &ka_cnt);
        else if (!strcmp(argv[i], "--idle")) idle = 1;
    }
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    start = t0.tv_sec + t0.tv_nsec / 1e9;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port) };
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (ka_idle) {
        int on = 1;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof on);
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &ka_idle, sizeof ka_idle);   // idle before 1st probe
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &ka_intvl, sizeof ka_intvl); // between probes
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &ka_cnt, sizeof ka_cnt);       // unanswered = dead
        printf("  keepalive on: first probe after %ds idle, then every %ds, dead after %d misses\n",
               ka_idle, ka_intvl, ka_cnt);
    }

    // ---- connect timeout: non-blocking connect + poll() with a deadline ------------------
    printf("  connecting to %s:%d (%s)\n", host, port,
           connect_ms ? "with a connect timeout" : "no connect timeout - kernel default");
    if (connect_ms) fcntl(fd, F_SETFL, O_NONBLOCK);
    int rc = connect(fd, (struct sockaddr *)&addr, sizeof addr);
    if (rc < 0 && errno == EINPROGRESS) {
        struct pollfd p = { .fd = fd, .events = POLLOUT };
        if (poll(&p, 1, connect_ms) == 0) { report("CONNECT TIMEOUT - giving up on the handshake"); return 1; }
        int err = 0;
        socklen_t len = sizeof err;
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err) { errno = err; rc = -1; } else rc = 0;
    }
    if (rc < 0) {
        char msg[128];
        snprintf(msg, sizeof msg, "connect failed: %s", strerror(errno));
        report(msg);
        return 1;
    }
    fcntl(fd, F_SETFL, 0);  // back to blocking for the rest
    report("connected");

    // ---- read timeout: SO_RCVTIMEO makes a blocking recv() give up with EAGAIN -----------
    if (read_ms) {
        struct timeval tv = { read_ms / 1000, (read_ms % 1000) * 1000 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    }
    if (!idle) {
        if (write(fd, "ping\n", 5) != 5) { report("write failed"); return 1; }
        report("sent request, waiting for the reply...");
    } else {
        report("idle - sitting on the connection like a pooled client...");
    }

    char buf[256];
    ssize_t n = recv(fd, buf, sizeof buf - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        char msg[300];
        snprintf(msg, sizeof msg, "reply: %s", buf);
        report(msg);
    } else if (n == 0) {
        report("server closed the connection");
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        report("READ TIMEOUT - no reply in time, the caller can retry or fail fast");
    } else if (errno == ETIMEDOUT) {
        report("ETIMEDOUT - keepalive probes went unanswered, connection declared dead");
    } else {
        char msg[128];
        snprintf(msg, sizeof msg, "recv failed: %s", strerror(errno));
        report(msg);
    }
    close(fd);
    return 0;
}
