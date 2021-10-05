#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

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

void test3_testf(float v) {
    assert(v == 10.1f);
}

void test3_good(void) {
    printf("test3_good: 10.1 == %f\n", 10.1f);
}

void test3_bad(void) {
    volatile float v = 10.1f;
    printf("test3_bad: 10.1 == %f\n", v);
}

void test3(void) {
    printf("test3: 10.1 == %f\n", 10.1f);
    test3_testf(10.1f);

    volatile float v = 10.1f;
    printf("test3: 10.1 == %f\n", v);
    test3_testf(v);

    test3_good();
    test3_bad();
}

float test4_t(double v) {
    printf("test4_t: v = %f\n", v);
    return v;
}

void test4(void) {
    assert(test4_t(10.6l) == 10.6f);
}

int main(int argc, char *argv[]) {
    printf("Hello World from C + libc\n");

    assert(strstr("1234", "34") != NULL);

    {
        volatile float v1 = 1.23f;
        int64_t v2;
        assert(sscanf("10.1 12345", "%f %ld", &v1, &v2) == 2);
        printf("v1 = %f\n", v1); /* TODO: currently delivers wrong values when build with --interpreter-only=true */
        printf("v2 = %ld\n", v2);
        assert(v1 == 10.100000f);
        assert(v2 == 12345);

        printf("nan = %f\n", NAN);
        printf("inf = %f\n", INFINITY);
        printf("1.0 = %f\n", 1.0f);
        printf("2.0 = %f\n", 2.0f);
        printf("10.1 = %f\n", 10.1f);
    }

    {
        volatile float v1 = 10.1f;
        volatile float v2 = 10.1f;
        volatile float v3 = 10.3f;

        assert(v1 == v2);
        assert(v1 != v3);
        assert(v2 != v3);

        printf("10.1 = %f\n", v1);
    }

    test3();
    test4();

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
