#include "mathx.h"

int calls = 0;  // a global with external linkage. zero-initialized, so it lands in .bss and
                // `nm` shows it as B (a non-zero initial value would make it D, for .data)

int square(int x) {
    calls++;
    return x * x;
}

int cube(int x) {
    return square(x) * x;
}
