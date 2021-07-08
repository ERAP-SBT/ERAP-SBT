#include "generator/x86_64/generator.h"
#include "test_irs.h"
#include "util.h"

#include <gtest/gtest.h>

using generator::x86_64::Generator;

// These sanity checks test whether the generator runs without hitting an assertion, and that it produces some output.

static void run_generator(File &output, void (*ir_generator)(IR &)) {
    IR ir;
    ir_generator(ir);

    Generator gen(&ir, {}, output.handle());
    gen.compile();
}

TEST(GeneratorBasicIR, print) {
    Buffer buf;
    {
        auto file = buf.open();
        run_generator(file, gen_print_ir);
    }
    ASSERT_FALSE(buf.view().empty());
}

TEST(GeneratorBasicIR, unreachable) {
    Buffer buf;
    {
        auto file = buf.open();
        run_generator(file, gen_unreachable_ir);
    }
    ASSERT_FALSE(buf.view().empty());
}

TEST(GeneratorBasicIR, syscall) {
    Buffer buf;
    {
        auto file = buf.open();
        run_generator(file, gen_syscall_ir);
    }
    ASSERT_FALSE(buf.view().empty());
}

TEST(GeneratorBasicIR, third) {
    Buffer buf;
    {
        auto file = buf.open();
        run_generator(file, gen_third_ir);
    }
    ASSERT_FALSE(buf.view().empty());
}

TEST(GeneratorBasicIR, sec) {
    Buffer buf;
    {
        auto file = buf.open();
        run_generator(file, gen_sec_ir);
    }
    ASSERT_FALSE(buf.view().empty());
}

TEST(GeneratorBasicIR, first) {
    Buffer buf;
    {
        auto file = buf.open();
        run_generator(file, gen_first_ir);
    }
    ASSERT_FALSE(buf.view().empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
