// same virtual address, two different values: proof that a virtual address only means
// something inside one process's page tables. fork() shares physical pages copy-on-write,
// and the first write from either side is what forces the kernel to make a private copy.
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

int value = 100;  // in .data - one page that parent and child both map after fork()

static long minor_faults(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_minflt;
}

int main(void) {
    printf("parent (pid %d): &value = %p, value = %d\n", getpid(), (void *)&value, value);
    fflush(stdout);  // otherwise the child inherits the unflushed buffer and prints it twice

    pid_t pid = fork();
    if (pid == 0) {
        // child: same address space LAYOUT, but its own page tables pointing at shared,
        // read-only-marked physical pages
        long before = minor_faults();
        value = 200;  // write -> page fault -> kernel copies the page -> write retried
        long after = minor_faults();
        printf("child  (pid %d): &value = %p, value = %d   <- same address, new value\n",
               getpid(), (void *)&value, value);
        printf("                 the write cost %ld page fault(s): that's copy-on-write\n",
               after - before);
        fflush(stdout);  // _exit() skips stdio cleanup, so flush by hand
        _exit(0);
    }
    waitpid(pid, NULL, 0);
    printf("parent (pid %d): &value = %p, value = %d   <- untouched\n",
           getpid(), (void *)&value, value);
    return 0;
}
