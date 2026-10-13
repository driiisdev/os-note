// fires N clients at a server all at once. each client connects, waits for the server's
// greeting, "thinks" for a while before sending its line (like a slow mobile client),
// then waits for the echo.
//   ./loadgen <port> [clients=10] [think_ms=500]
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int port, think_ms;
static double start;

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void *client(void *arg) {
    long id = (long)arg;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port),
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("connect"); return NULL; }

    char msg[64], reply[128] = { 0 }, hello[8];
    // connect() succeeding only means the KERNEL finished the handshake. the greeting proves
    // the server application has actually accept()ed us and is paying attention
    if (read(fd, hello, sizeof hello) <= 0) { close(fd); return NULL; }
    usleep(think_ms * 1000);                          // slow client: connected but silent

    int len = snprintf(msg, sizeof msg, "hello from client %ld\n", id);
    if (write(fd, msg, len) < 0) perror("write");
    ssize_t n = read(fd, reply, sizeof reply - 1);
    double done = now_ms() - start;
    printf("  client %2ld answered after %6.0f ms: %s", id, done, n > 0 ? reply : "(nothing)\n");
    close(fd);
    return (void *)(long)done;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <port> [clients] [think_ms]\n", argv[0]); return 1; }
    port = atoi(argv[1]);
    int clients = argc > 2 ? atoi(argv[2]) : 10;
    think_ms = argc > 3 ? atoi(argv[3]) : 500;

    pthread_t th[256];
    start = now_ms();
    for (long i = 0; i < clients && i < 256; i++) pthread_create(&th[i], NULL, client, (void *)i);
    for (int i = 0; i < clients && i < 256; i++) pthread_join(th[i], NULL);
    printf("  => %d clients x %dms think time, all served in %.0f ms\n",
           clients, think_ms, now_ms() - start);
    return 0;
}
