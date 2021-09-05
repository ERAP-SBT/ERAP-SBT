#include "ir/eval.h"

#include "gtest/gtest.h"
#include <cstdint>

TEST(TestEval, test_eval_binary) {
    struct TestTuple {
        Instruction insn;
        Type type;
        uint64_t a;
        uint64_t b;
        uint64_t expected;
    };

    auto cases = {
        TestTuple{Instruction::add, Type::i32, 1, 1, 2},
        TestTuple{Instruction::add, Type::i32, UINT32_MAX, 1, 0},
        TestTuple{Instruction::add, Type::i32, 5, UINT32_MAX, 4},
        TestTuple{Instruction::sub, Type::i32, 5, 4, 1},
        TestTuple{Instruction::sub, Type::i32, 1, 2, UINT32_MAX},
        TestTuple{Instruction::sub, Type::i32, 0, 0, 0},
        TestTuple{Instruction::mul_l, Type::i32, 2, 2, 4},
        TestTuple{Instruction::ssmul_h, Type::i32, (uint32_t)INT32_MIN, 16, (uint32_t)-8},
        TestTuple{Instruction::uumul_h, Type::i32, (uint32_t)INT32_MIN, 16, (uint32_t)8},
        TestTuple{Instruction::sumul_h, Type::i32, (uint32_t)INT32_MIN, 16, (uint32_t)-8},
        TestTuple{Instruction::shl, Type::i32, 1, 1, 2},
        TestTuple{Instruction::shr, Type::i32, 4, 1, 2},
        TestTuple{Instruction::sar, Type::i32, 4, 1, 2},
        TestTuple{Instruction::sar, Type::i32, (uint32_t)0b1 << 31, 1, (uint32_t)0b11 << 30},
        TestTuple{Instruction::_or, Type::i32, 1, 2, 3},
        TestTuple{Instruction::_or, Type::i32, 1, 0, 1},
        TestTuple{Instruction::_and, Type::i32, 1, 1, 1},
        TestTuple{Instruction::_and, Type::i32, 1, 0, 0},
        TestTuple{Instruction::_xor, Type::i32, 1, 1, 0},
        TestTuple{Instruction::_xor, Type::i32, 1, 0, 1},
    };

    for (const auto &test_case : cases) {
        ASSERT_EQ(eval_binary_op(test_case.insn, test_case.type, test_case.a, test_case.b), test_case.expected);
    }
}

TEST(TestEval, test_eval_unary) { ASSERT_EQ(eval_unary_op(Instruction::_not, Type::i32, 1), 0xFFFFFFFE); }

TEST(TestEval, test_eval_morph) {
    struct TestTuple {
        Instruction insn;
        Type in_type;
        uint64_t in;
        Type out_type;
        uint64_t out;
    };

    auto cases = {
        TestTuple{Instruction::sign_extend, Type::i16, 0xFFFF, Type::i32, 0xFFFFFFFF},
        TestTuple{Instruction::sign_extend, Type::i16, 0x7FFF, Type::i32, 0x00007FFF},
        TestTuple{Instruction::zero_extend, Type::i16, 0xFFFF, Type::i32, 0x0000FFFF},
        TestTuple{Instruction::cast, Type::i32, 0xFFFFFFFF, Type::i16, 0xFFFF},
    };

    for (const auto &test_case : cases) {
        ASSERT_EQ(eval_morphing_op(test_case.insn, test_case.in_type, test_case.out_type, test_case.in), test_case.out);
    }
}

TEST(TestEval, test_eval_div) {
    {
        auto [div, rem] = eval_div(Instruction::div, Type::i32, 16, 5);
        ASSERT_EQ(div, 3);
        ASSERT_EQ(rem, 1);
    }
    {
        auto [div, rem] = eval_div(Instruction::div, Type::i32, -16, 5);
        ASSERT_EQ(div, (uint32_t)-3);
        ASSERT_EQ(rem, (uint32_t)-1);
    }
    {
        auto [div, rem] = eval_div(Instruction::udiv, Type::i32, 16, 5);
        ASSERT_EQ(div, 3);
        ASSERT_EQ(rem, 1);
    }
}

TEST(TestEval, test_utils) {
    ASSERT_TRUE(typed_equal(Type::i16, 0xFFFFFFFF, 0x0000FFFF));
    ASSERT_FALSE(typed_equal(Type::i16, 0xFFFFFFF0, 0x0000FFFF));

    ASSERT_EQ(typed_narrow(Type::i16, 0xFFFFFFFF), 0x0000FFFF);
}
