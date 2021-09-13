#include "ir/ir.h"
#include "ir/operation.h"
#include "ir/optimizer/const_folding.h"

#include "gtest/gtest.h"

using namespace optimizer;

static void assert_valid(const IR &ir) {
    std::vector<std::string> messages;
    if (!ir.verify(messages)) {
        for (const auto &msg : messages) {
            std::cerr << msg << '\n';
        }
        GTEST_FAIL() << "IR is not valid";
    }
}

TEST(TestConstFolding, test_basic_eval) {
    IR ir;
    auto *bb = ir.add_basic_block();

    auto *a = bb->add_var_imm(13, 0);
    auto *b = bb->add_var_imm(24, 1);

    auto *c = bb->add_var(Type::i32, 2);
    c->set_op(Operation::new_add(c, a, b));

    assert_valid(ir);

    const_fold(&ir);

    assert_valid(ir);

    ASSERT_TRUE(c->is_immediate());
    ASSERT_EQ(c->get_immediate().val, 37);
}

TEST(TestConstFolding, test_eval_twice) {
    IR ir;
    auto *bb = ir.add_basic_block();

    auto *a = bb->add_var_imm(-5, 0);
    auto *b = bb->add_var_imm(2, 1);

    auto *c = bb->add_var(Type::i16, 2);
    c->set_op(Operation::new_add(c, a, b));

    auto *d = bb->add_var(Type::i32, 4);
    d->set_op(Operation::new_sign_extend(d, c));

    assert_valid(ir);

    const_fold(&ir);

    assert_valid(ir);

    ASSERT_TRUE(c->is_immediate());
    ASSERT_EQ(c->get_immediate().val, (uint16_t)-3);

    ASSERT_TRUE(d->is_immediate());
    ASSERT_EQ(d->get_immediate().val, (uint32_t)-3);
}

TEST(TestConstFolding, test_simplify) {
    IR ir;
    ir.add_static(Type::i32);
    auto *bb = ir.add_basic_block();
    auto *bb2 = ir.add_basic_block();

    auto *a = bb->add_var_from_static(0);
    auto *b = bb->add_var_imm(0, 0);

    auto *c = bb->add_var(Type::i32, 1);
    c->set_op(Operation::new_add(c, a, b));

    auto &o = bb->add_cf_op(CFCInstruction::jump, bb2);
    o.add_target_input(c, 0);

    assert_valid(ir);

    const_fold(&ir);

    assert_valid(ir);

    ASSERT_EQ(o.target_inputs()[0]->id, a->id);
}

enum class Side {
    Any,
    Left,
    Right,
};

struct DoubleTestCase {
    Instruction first_insn;
    uint64_t first_imm;
    Side first_imm_side;

    Instruction second_insn;
    uint64_t second_imm;
    Side second_imm_side;

    Instruction result_insn;
    uint64_t result_imm;
    Side result_imm_side;
};

// operator<< implementations for assertion pretty printing

std::ostream &operator<<(std::ostream &out, Side side) {
    auto side_name = [](Side side) -> const char * {
        switch (side) {
        case Side::Any:
            return "any";
        case Side::Left:
            return "left";
        case Side::Right:
            return "right";
        }
        __builtin_unreachable();
    };
    out << side_name(side);
    return out;
}

std::ostream &operator<<(std::ostream &out, const DoubleTestCase &tc) {
    out << "{ first: {" << tc.first_insn << ", " << tc.first_imm << ", " << tc.first_imm_side << "}, "
        << "second: {" << tc.second_insn << ", " << tc.second_imm << ", " << tc.second_imm_side << "}, "
        << "result: {" << tc.result_insn << ", " << tc.result_imm << ", " << tc.result_imm_side << "} }";
    return out;
}

class DoubleFoldTest : public testing::TestWithParam<DoubleTestCase> {};

TEST_P(DoubleFoldTest, DoesFoldIntoOneVar) {
    DoubleTestCase test_case = GetParam();

    assert(test_case.first_imm_side != Side::Any && test_case.second_imm_side != Side::Any);

    IR ir;
    ir.add_static(Type::i32);
    auto *bb = ir.add_basic_block();

    auto *first_stc = bb->add_var_from_static(0);
    auto *first_imm = bb->add_var_imm(test_case.first_imm, 0);

    auto *first = bb->add_var(Type::i32, 1);
    {
        auto op = std::make_unique<Operation>(test_case.first_insn);
        op->set_outputs(first);
        if (test_case.first_imm_side == Side::Left) {
            op->set_inputs(first_imm, first_stc);
        } else {
            op->set_inputs(first_stc, first_imm);
        }
        first->set_op(std::move(op));
    }

    auto *second_imm = bb->add_var_imm(test_case.second_imm, 2);

    auto *second = bb->add_var(Type::i32, 3);
    {
        auto op = std::make_unique<Operation>(test_case.second_insn);
        op->set_outputs(second);
        if (test_case.second_imm_side == Side::Left) {
            op->set_inputs(second_imm, first);
        } else {
            op->set_inputs(first, second_imm);
        }
        second->set_op(std::move(op));
    }

    assert_valid(ir);

    const_fold(&ir);

    assert_valid(ir);

    {
        auto *result = second;
        ASSERT_TRUE(result->is_operation());

        const auto &op = result->get_operation();
        ASSERT_TRUE(op.in_vars[0] && op.in_vars[1]);

        SSAVar *result_imm, *result_stc;
        Side result_imm_side;
        if (op.in_vars[0]->is_immediate()) {
            result_imm = op.in_vars[0].get();
            result_stc = op.in_vars[1].get();
            result_imm_side = Side::Left;
        } else {
            result_imm = op.in_vars[1].get();
            result_stc = op.in_vars[0].get();
            result_imm_side = Side::Right;
        }

        ASSERT_TRUE(result_imm->is_immediate());
        ASSERT_EQ(result_imm->get_immediate().val, test_case.result_imm);
        ASSERT_TRUE(result_stc->is_static());
        ASSERT_EQ(result_stc->get_static(), first_stc->get_static());

        ASSERT_EQ(op.type, test_case.result_insn);

        if (test_case.result_imm_side != Side::Any) {
            ASSERT_EQ(result_imm_side, test_case.result_imm_side);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(DoubleInst, DoubleFoldTest,
                         testing::ValuesIn({
                             DoubleTestCase{Instruction::add, 1, Side::Right, Instruction::add, 2, Side::Right, Instruction::add, 3, Side::Any},
                             DoubleTestCase{Instruction::add, 1, Side::Left, Instruction::add, 2, Side::Right, Instruction::add, 3, Side::Any},
                             DoubleTestCase{Instruction::add, 1, Side::Right, Instruction::add, 2, Side::Left, Instruction::add, 3, Side::Any},
                             DoubleTestCase{Instruction::add, 1, Side::Left, Instruction::add, 2, Side::Left, Instruction::add, 3, Side::Any},

                             DoubleTestCase{Instruction::add, 6, Side::Right, Instruction::sub, 4, Side::Right, Instruction::add, 2, Side::Any},
                             DoubleTestCase{Instruction::add, 6, Side::Left, Instruction::sub, 4, Side::Right, Instruction::add, 2, Side::Any},
                             DoubleTestCase{Instruction::add, 4, Side::Right, Instruction::sub, 7, Side::Left, Instruction::sub, 3, Side::Left},
                             DoubleTestCase{Instruction::add, 4, Side::Left, Instruction::sub, 7, Side::Left, Instruction::sub, 3, Side::Left},

                             DoubleTestCase{Instruction::sub, 4, Side::Right, Instruction::add, 7, Side::Right, Instruction::add, 3, Side::Any},
                             DoubleTestCase{Instruction::sub, 4, Side::Right, Instruction::add, 7, Side::Left, Instruction::add, 3, Side::Any},
                             DoubleTestCase{Instruction::sub, 6, Side::Left, Instruction::add, 4, Side::Right, Instruction::sub, 10, Side::Left},
                             DoubleTestCase{Instruction::sub, 6, Side::Left, Instruction::add, 4, Side::Left, Instruction::sub, 10, Side::Left},

                             DoubleTestCase{Instruction::sub, 4, Side::Right, Instruction::sub, 5, Side::Right, Instruction::sub, 9, Side::Right},
                             DoubleTestCase{Instruction::sub, 6, Side::Left, Instruction::sub, 4, Side::Right, Instruction::sub, 2, Side::Left},
                             DoubleTestCase{Instruction::sub, 3, Side::Right, Instruction::sub, 2, Side::Left, Instruction::sub, 5, Side::Left},
                             DoubleTestCase{Instruction::sub, 6, Side::Left, Instruction::sub, 9, Side::Left, Instruction::add, 3, Side::Left},

                             DoubleTestCase{Instruction::_and, 0b110, Side::Right, Instruction::_and, 0b011, Side::Right, Instruction::_and, 0b010, Side::Any},
                             DoubleTestCase{Instruction::_or, 0b110, Side::Right, Instruction::_or, 0b011, Side::Right, Instruction::_or, 0b111, Side::Any},
                             DoubleTestCase{Instruction::_xor, 0b110, Side::Right, Instruction::_xor, 0b011, Side::Right, Instruction::_xor, 0b101, Side::Any},
                         }));
