// the "interface" of our tiny library: declarations only. the compiler needs these to
// type-check calls; the LINKER is what later finds the actual code.
#ifndef MATHX_H
#define MATHX_H

#define MATHX_VERSION "1.0"

int square(int x);
int cube(int x);

#endif
