#include <stdio.h>
#include "mathx.h"

#define N 3

extern int calls;  // defined in mathx.c - main.o only holds an unresolved reference to it

int main(void) {
    int s = square(N);
    int c = cube(N);
    // after preprocessing, N and MATHX_VERSION are gone: replaced by 3 and "1.0"
    printf("mathx %s: square(%d) = %d, cube(%d) = %d, square() ran %d times\n",
           MATHX_VERSION, N, s, N, c, calls);
    return 0;
}
