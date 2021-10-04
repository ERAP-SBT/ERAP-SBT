#include "ir/ir.h"
#include "ir/optimizer/dce.h"
#include "ir/optimizer/dedup.h"
#include "shared.h"

#include "gtest/gtest.h"

using namespace optimizer;

TEST(TestDce, dce_removes_variables) {
    IR ir;
    auto *bb = ir.add_basic_block();

    bb->add_var_imm(13, 0);
    ASSERT_EQ(bb->variables.size(), 1);

    dce(&ir);

    assert_valid(ir);
    ASSERT_TRUE(bb->variables.empty());
}

TEST(TestDedupImm, deduplicates_immediate) {
    IR ir;
    auto *bb = ir.add_basic_block();

    auto *a = bb->add_var_imm(12, 0);
    auto *b = bb->add_var_imm(12, 1);

    auto *c = bb->add_var(Type::i32, 2);
    c->set_op(Operation::new_add(c, a, b));

    assert_valid(ir);

    dedup(&ir);

    assert_valid(ir);
    const auto &op = c->get_operation();
    ASSERT_EQ(op.in_vars[0].get(), a);
    ASSERT_EQ(op.in_vars[1].get(), a);

    // the duplicate should be removed
    ASSERT_EQ(bb->variables.size(), 2);
}
