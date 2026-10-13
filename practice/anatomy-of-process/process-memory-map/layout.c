// prints where every kind of variable actually lives inside this process's address space,
// sorted low -> high, then dumps /proc/self/maps so you can match each address to a region
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int initialized_global = 42;          // .data  - has a non-zero initial value, stored in the file
int zeroed_global;                    // .bss   - zero-initialized, takes NO space in the file
static int zeroed_array[4096];        // .bss   - 16KB of zeros the loader just maps as zero pages
const int read_only_const = 7;        // .rodata - writing to it would segfault
const char *greeting = "hello";       // pointer lives in .data, the literal itself in .rodata

struct entry { const char *label; const void *addr; };

static int by_addr(const void *a, const void *b) {
    const struct entry *x = a, *y = b;
    return (x->addr > y->addr) - (x->addr < y->addr);
}

int main(int argc, char **argv) {
    (void)argc;
    static int static_local = 1;      // .data - "local" by scope, but NOT on the stack
    int stack_local = 1;              // stack  - lives in main's stack frame
    int *heap_small = malloc(64);     // heap   - small request, served from the brk heap
    char *heap_big = malloc(8 << 20); // mmap   - 8MB is above glibc's mmap threshold (128KB)
    zeroed_array[0] = initialized_global + zeroed_global; // keep the compiler from dropping it

    struct entry e[] = {
        { "text    main()",             (void *)main },
        { "rodata  read_only_const",    &read_only_const },
        { "rodata  \"hello\" literal",  greeting },
        { "data    initialized_global", &initialized_global },
        { "data    static_local",       &static_local },
        { "bss     zeroed_global",      &zeroed_global },
        { "bss     zeroed_array",       zeroed_array },
        { "heap    malloc(64)",         heap_small },
        { "mmap    malloc(8MB)",        heap_big },
        { "stack   stack_local",        &stack_local },
        { "stack   argv[0] string",     argv[0] },
    };
    size_t n = sizeof e / sizeof e[0];
    qsort(e, n, sizeof e[0], by_addr);

    printf("pid %d - addresses, lowest first:\n\n", getpid());
    for (size_t i = 0; i < n; i++)
        printf("  %-28s %p\n", e[i].label, e[i].addr);

    printf("\n/proc/self/maps (the kernel's view of the same address space):\n\n");
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    while (f && fgets(line, sizeof line, f)) fputs(line, stdout);
    if (f) fclose(f);

    if (getenv("PAUSE")) {
        printf("\npaused - in another terminal: cat /proc/%d/maps   (Enter to exit)\n", getpid());
        getchar();
    }
    free(heap_small);
    free(heap_big);
    return 0;
}
