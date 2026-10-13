// the smallest program worth stepping through instruction by instruction:
// main builds a frame, calls add (which builds its own frame), gets the result back in eax
int add(int a, int b) {
    int c = a + b;
    return c;
}

int main(void) {
    int x = 1;
    int y = 2;
    int z = add(x, y);
    return z;   // `echo $?` after running prints 3
}
