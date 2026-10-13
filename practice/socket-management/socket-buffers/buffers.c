// what send() and recv() really do: they copy bytes into and out of KERNEL buffers.
// one process, one loopback TCP connection, two experiments:
//   1) a receiver that never reads - how much can the sender send() before it blocks?
//   2) three separate send() calls - how many recv() calls does it take to read them?
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

// a connected (client, server) pair over 127.0.0.1
static void tcp_pair(int *client, int *server) {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    socklen_t len = sizeof addr;
    bind(lfd, (struct sockaddr *)&addr, sizeof addr);   // port 0 = let the kernel pick one
    listen(lfd, 1);
    getsockname(lfd, (struct sockaddr *)&addr, &len);

    *client = socket(AF_INET, SOCK_STREAM, 0);
    connect(*client, (struct sockaddr *)&addr, sizeof addr);  // handshake completes here
    *server = accept(lfd, NULL, NULL);                        // ...and is picked up here
    close(lfd);
}

static int opt(int fd, int name) {
    int v;
    socklen_t l = sizeof v;
    getsockopt(fd, SOL_SOCKET, name, &v, &l);
    return v;
}

static int queued(int fd, unsigned long req) {
    int n = 0;
    ioctl(fd, req, &n);  // SIOCOUTQ: bytes not yet ACKed by the peer, SIOCINQ: unread bytes
    return n;
}

int main(void) {
    int c, s;
    tcp_pair(&c, &s);
    printf("1) a receiver that never calls recv()\n");
    printf("   client SO_SNDBUF = %d KB, server SO_RCVBUF = %d KB\n",
           opt(c, SO_SNDBUF) / 1024, opt(s, SO_RCVBUF) / 1024);

    fcntl(c, F_SETFL, O_NONBLOCK);  // instead of blocking when the buffers fill, fail with EAGAIN
    char chunk[16384];
    memset(chunk, 'x', sizeof chunk);
    long total = 0;
    for (;;) {
        ssize_t n = send(c, chunk, sizeof chunk, 0);
        if (n > 0) { total += n; continue; }
        if (errno == EAGAIN) break;   // both buffers are full - a blocking socket would sleep here
        perror("send");
        return 1;
    }
    usleep(100 * 1000);  // let in-flight segments land in the receiver's buffer
    printf("   send() accepted %ld KB before returning EAGAIN - and the server read NOTHING\n",
           total / 1024);
    printf("   where the bytes are sitting right now (all inside the kernel):\n");
    printf("     client send queue   (SIOCOUTQ): %6d KB  - sent, waiting for the peer\n",
           queued(c, SIOCOUTQ) / 1024);
    printf("     server receive queue (SIOCINQ): %6d KB  - arrived, waiting for recv()\n",
           queued(s, SIOCINQ) / 1024);

    char big[1 << 20];
    ssize_t got = recv(s, big, sizeof big, 0);
    usleep(100 * 1000);
    ssize_t again = send(c, chunk, sizeof chunk, 0);
    printf("   server finally recv()s %zd KB -> window opens -> client send() works again: %zd bytes\n\n",
           got / 1024, again);
    close(c);
    close(s);

    printf("2) TCP is a byte stream - it does not keep your message boundaries\n");
    tcp_pair(&c, &s);
    send(c, "A", 1, 0);
    send(c, "BB", 2, 0);
    send(c, "CCC", 3, 0);
    printf("   client: send(\"A\"), send(\"BB\"), send(\"CCC\")  (3 calls)\n");
    usleep(50 * 1000);
    char buf[64] = { 0 };
    ssize_t n = recv(s, buf, sizeof buf - 1, 0);
    printf("   server: one recv() returned %zd bytes: \"%s\"\n", n, buf);
    printf("   -> real protocols add framing: a length prefix, or a delimiter like \\r\\n\n");
    close(c);
    close(s);
    return 0;
}
