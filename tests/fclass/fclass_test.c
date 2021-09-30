#include <math.h>
#include <stdio.h>

typedef union conv {
    unsigned int d32;
    unsigned long d64;
    float f32;
    double f64;
} conv;

void test_val_f(float val) {
    unsigned long res;
    __asm__ __volatile__("fclass.s %0, %1" : "=r"(res) : "f"(val));
    conv c = {f32 : val};
    printf("%08x : %lx\n", c.d32, res);
}

void test_val_d(double val) {
    unsigned long res;
    __asm__ __volatile__("fclass.d %0, %1" : "=r"(res) : "f"(val));
    conv c = {f64 : val};
    printf("%016lx : %lx\n", c.d64, res);
}

void test_floats() {
    conv neg_inf = {d32 : 0xff800000};
    conv pos_inf = {d32 : 0x7f800000};
    conv snan = {d32 : 0x7fa00000};
    conv qnan = {d32 : 0x7fc00000};
    printf("FLOATS:\n");
    printf("  Input  | 'fclass.s' result (hex)\n");
    test_val_f(neg_inf.f32);
    test_val_f(-810);
    test_val_f(-5.87747175411e-39);
    test_val_f(-0.0);
    test_val_f(0.0);
    test_val_f(5.87747175411e-39);
    test_val_f(810);
    test_val_f(pos_inf.f32);
    test_val_f(snan.f32);
    test_val_f(qnan.f32);
}

void test_doubles() {
    conv neg_inf = {d64 : 0xfff0000000000000};
    conv pos_inf = {d64 : 0x7ff0000000000000};
    conv snan = {d64 : 0x7ff4000000000000};
    conv qnan = {d64 : 0x7ff8000000000000};
    printf("DOUBLES: \n");
    printf("      Input      | 'fclass.d' result (hex)\n");
    test_val_d(neg_inf.f64);
    test_val_d(-469);
    test_val_d(-1.11253692925360069154511635867E-308);
    test_val_d(-0.0);
    test_val_d(0.0);
    test_val_d(1.11253692925360069154511635867E-308);
    test_val_d(469);
    test_val_d(pos_inf.f64);
    test_val_d(snan.f64);
    test_val_d(qnan.f64);
}

int main() {
    test_floats();
    printf("\n");
    test_doubles();
    return 0;
}
