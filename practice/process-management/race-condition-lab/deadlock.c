// two threads, two locks, opposite order -> each holds one lock and waits forever for the
// other. a watchdog alarm reports the deadlock instead of hanging your terminal.
//   ./deadlock           thread 1 takes A then B, thread 2 takes B then A  -> deadlock
//   ./deadlock ordered   both take A then B (a global lock order)          -> always finishes
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static int ordered;

static void *transfer(void *arg) {
    int id = *(int *)arg;
    // thread 2 normally grabs the locks in the opposite order - the classic bug
    pthread_mutex_t *first = (id == 1 || ordered) ? &A : &B;
    pthread_mutex_t *second = (id == 1 || ordered) ? &B : &A;

    pthread_mutex_lock(first);
    printf("thread %d: holds %s, wants %s\n", id, first == &A ? "A" : "B",
           second == &A ? "A" : "B");
    usleep(100 * 1000);  // widen the window so both threads grab their first lock
    pthread_mutex_lock(second);
    printf("thread %d: got both locks, doing work\n", id);
    pthread_mutex_unlock(second);
    pthread_mutex_unlock(first);
    return NULL;
}

static void watchdog(int sig) {
    (void)sig;
    const char msg[] = "\nDEADLOCK: no progress for 2s - each thread holds the lock the other "
                       "needs (hold-and-wait + circular wait)\n";
    if (write(STDOUT_FILENO, msg, sizeof msg - 1) < 0) _exit(2);
    _exit(1);
}

int main(int argc, char **argv) {
    ordered = argc > 1 && strcmp(argv[1], "ordered") == 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGALRM, watchdog);
    alarm(2);

    int one = 1, two = 2;
    pthread_t t1, t2;
    pthread_create(&t1, NULL, transfer, &one);
    pthread_create(&t2, NULL, transfer, &two);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("both threads finished - a single global lock order makes circular wait impossible\n");
    return 0;
}
