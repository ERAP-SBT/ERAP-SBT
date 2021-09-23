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

#if 1
/* This NOP causes the lifter to not recognize a basic block correctly, forcing the interpreter to run
 */
__attribute__((naked)) void f1(void) { __asm__ volatile("nop" : : : "memory"); }
#endif

static void fib_test(uint32_t n) {
    printf("Calculating fib(%u): ", n);
    fflush(stdout);
    printf("%lu\n", fib(n));
}

typedef void (*func_type)(uint32_t n);

// volatile: force compiler to write value into memory and read it back, so this won't be recognized as
// basicblock
volatile func_type a;

int main(int argc, char *argv[]) {
    printf("Hello World from C + musl libc\n");

    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; ++i) {
        printf("argv[%u]: %s\n", i, argv[i]);
    }

    a = &fib_test;

    __asm__ volatile("nop" : : : "memory");

    a(2);
    a(35);
    return EXIT_SUCCESS;
}
