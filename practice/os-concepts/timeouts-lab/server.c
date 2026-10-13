// a server that misbehaves on purpose, to give the client's timeouts something to catch.
//   ./server normal   read a line, echo it back
//   ./server hang     read the request... and never answer   (a stuck backend)
//   ./server silent   accept, then never read or write        (an idle pooled connection)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9300

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "normal";
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(PORT),
                                .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    listen(lfd, 16);
    printf("server (%s) on :%d\n", mode, PORT);
    fflush(stdout);

    for (;;) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) continue;
        if (strcmp(mode, "silent") == 0) continue;   // keep the fd open, never touch it

        char buf[256];
        ssize_t n = read(cfd, buf, sizeof buf);
        if (strcmp(mode, "hang") == 0) continue;     // got the request, will never reply
        if (n > 0 && write(cfd, buf, n) < 0) perror("write");
        close(cfd);
    }
}
