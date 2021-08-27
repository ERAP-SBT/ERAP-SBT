#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t fib(uint32_t n) {
    if (n <= 1) {
        return n;
    } else {
        return fib(n - 2) + fib(n - 1);
    }
}

static void fib_test(uint32_t n) {
    printf("Calculating fib(%u): ", n);
    fflush(stdout);
    printf("%lu\n", fib(n));
}

int main(int argc, char *argv[]) {
    printf("Hello World from C + musl libc\n");

    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; ++i) {
        printf("argv[%u]: %s\n", i, argv[i]);
    }

    fib_test(35);

    return EXIT_SUCCESS;
}
