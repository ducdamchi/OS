#include <stdio.h>

int main(int argc, char **argv) {
    int i;
    for (i = 0; i < 10; ++i) {
        fprintf(stdout, "(out): line %d\n", i);
        fprintf(stderr, "(err): line %d\n", i);
    }
    return 0;
}
