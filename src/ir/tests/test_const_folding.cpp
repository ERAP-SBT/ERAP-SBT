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

TEST(TestConstFolding, test_double) {
    IR ir;
    ir.add_static(Type::i32);
    auto *bb = ir.add_basic_block();

    auto *a = bb->add_var_from_static(0);
    auto *b = bb->add_var_imm(1, 0);

    auto *c = bb->add_var(Type::i32, 1);
    c->set_op(Operation::new_add(c, a, b));

    auto *d = bb->add_var_imm(2, 2);

    auto *e = bb->add_var(Type::i32, 3);
    e->set_op(Operation::new_add(e, c, d));

    assert_valid(ir);

    const_fold(&ir);

    assert_valid(ir);

    ASSERT_TRUE(e->is_operation());
    const auto &op = e->get_operation();

    ASSERT_TRUE(op.in_vars[1] && op.in_vars[1]->is_static());
    ASSERT_EQ(op.in_vars[1]->get_static(), 0);

    ASSERT_TRUE(op.in_vars[0] && op.in_vars[0]->is_immediate());
    ASSERT_EQ(op.in_vars[0]->get_immediate().val, 3);
}
