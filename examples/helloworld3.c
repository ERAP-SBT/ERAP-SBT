#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
    printf("Hello World from C + libc\n");

    assert(strstr("1234", "34") != NULL);

    {
        float v1;
        int64_t v2;
        assert(sscanf("10.1 12345", "%f %d", &v1, &v2) == 2);
        printf("v1 = %f\n", v1); /* TODO: currently delivers wrong values when build with --interpreter-only=true */
        printf("v2 = %d\n", v2);
        assert(v1 == 10.100000f);
        assert(v2 == 12345);
    }

    {
        volatile float v1 = 10.1f;
        volatile float v2 = 10.1f;
        volatile float v3 = 10.3f;

        assert(v1 == v2);
        assert(v1 != v3);
        assert(v2 != v3);
    }

    assert(strcmp("abc", "abc2") < 0);

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
