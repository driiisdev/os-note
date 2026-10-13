// three experiments on the stack:
//   ./stack frames    - each nested call gets a frame at a LOWER address (stack grows down)
//   ./stack overflow  - recurse until the stack limit (ulimit -s) is hit, and report how deep
//   ./stack dangling  - why returning a pointer to a local variable is a bug
#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

static char *top_of_stack;

// noinline so the compiler really creates one frame per call instead of flattening them
__attribute__((noinline)) static void frame(int depth, int max) {
    int local = depth;
    void *fp = __builtin_frame_address(0);   // this frame's base pointer (rbp)
    void *ret = __builtin_return_address(0); // where `ret` will jump back to in the caller
    printf("  depth %d: &local=%p  frame=%p  return-to=%p  (%ld bytes below main)\n",
           depth, (void *)&local, fp, ret, (long)(top_of_stack - (char *)&local));
    if (depth < max) frame(depth + 1, max);
}

// --- overflow -----------------------------------------------------------------------------
static volatile long deepest;
static volatile uintptr_t lowest;  // address of the deepest frame reached so far

static void on_segv(int sig) {
    (void)sig;
    // we're running on an alternate signal stack, because the real one is full.
    // only async-signal-safe calls in here: snprintf into a buffer + write()
    char msg[200];
    long used = (long)((uintptr_t)top_of_stack - lowest);
    int n = snprintf(msg, sizeof msg,
                     "  SIGSEGV at depth %ld - %ld KB of stack used, ~%ld bytes per frame\n",
                     deepest, used / 1024, used / deepest);
    write(STDOUT_FILENO, msg, n);
    _exit(0);
}

__attribute__((noinline)) static void recurse(long depth) {
    volatile char buf[1000];  // ~1KB per frame; volatile so it isn't optimized away
    buf[0] = (char)depth;
    deepest = depth;
    lowest = (uintptr_t)buf;
    if (depth == -1) return;  // never true - just stops gcc warning about infinite recursion
    recurse(depth + 1);
    buf[1] = buf[0];          // work after the call keeps this from becoming a tail call
}

// --- dangling -----------------------------------------------------------------------------
__attribute__((noinline)) static int *bad_pointer(void) {
    int value = 1234;
    int *p = &value;
    return p;                 // value's frame is popped the moment we return
}

__attribute__((noinline)) static int overwrite_stack(void) {
    volatile int junk[16];
    for (int i = 0; i < 16; i++) junk[i] = -1;  // reuses the exact bytes bad_pointer used
    return junk[0];
}

int main(int argc, char **argv) {
    int anchor;
    top_of_stack = (char *)&anchor;
    const char *mode = argc > 1 ? argv[1] : "frames";

    if (strcmp(mode, "frames") == 0) {
        printf("nested calls - watch the addresses go DOWN:\n");
        frame(1, 5);
    } else if (strcmp(mode, "overflow") == 0) {
        struct rlimit rl;
        getrlimit(RLIMIT_STACK, &rl);
        printf("stack limit (ulimit -s): %ld KB\n", (long)rl.rlim_cur / 1024);
        fflush(stdout);  // the handler exits with _exit(), which skips stdio's buffers

        // the handler needs its own stack, since the overflowing one has no room left
        static char altstack[64 * 1024];
        stack_t ss = { .ss_sp = altstack, .ss_size = sizeof altstack, .ss_flags = 0 };
        sigaltstack(&ss, NULL);
        struct sigaction sa = { .sa_handler = on_segv, .sa_flags = SA_ONSTACK };
        sigaction(SIGSEGV, &sa, NULL);
        recurse(1);
    } else if (strcmp(mode, "dangling") == 0) {
        int *p = bad_pointer();
        printf("right after return : *p = %d   (looks fine - the bytes weren't reused yet)\n", *p);
        (void)overwrite_stack();
        printf("after another call : *p = %d   (the next call's frame overwrote it)\n", *p);
    } else {
        fprintf(stderr, "usage: %s [frames|overflow|dangling]\n", argv[0]);
        return 1;
    }
    return 0;
}
