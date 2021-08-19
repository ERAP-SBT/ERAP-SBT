#include "ir/ir.h"
#include "ir/operation.h"

#include "gtest/gtest.h"
#include <algorithm>

TEST(TestIR, test_ir_op_creation) {
    IR ir;
    ir.setup_bb_addr_vec(0, 100);
    ir.add_static(Type::mt);
    ir.add_static(Type::i32);
    ir.add_static(Type::i32);

    uint64_t aa = 0;

    auto bb = ir.add_basic_block(0);
    auto mt = bb->add_var_from_static(0, aa++);
    auto v1 = bb->add_var_from_static(1, aa++), v2 = bb->add_var_from_static(2, aa++);

    auto v3 = bb->add_var(Type::mt, aa++);
    v3->set_op(Operation::new_store(v3, v1, v2, mt));

    auto v4 = bb->add_var(Type::i32, aa++);
    v4->set_op(Operation::new_load(v4, v1, v3));

    auto v5 = bb->add_var(Type::i32, aa++);
    v5->set_op(Operation::new_add(v5, v4, v2));

    auto v6 = bb->add_var(Type::i32, aa++);
    v6->set_op(Operation::new_sub(v6, v5, v4));

    auto v7 = bb->add_var(Type::i32, aa++);
    v7->set_op(Operation::new_mul_l(v7, v6, v5));

    auto v8 = bb->add_var(Type::i32, aa++);
    v8->set_op(Operation::new_ssmul_h(v8, v7, v6));

    auto v9 = bb->add_var(Type::i32, aa++);
    v9->set_op(Operation::new_uumul_h(v9, v8, v7));

    auto v10 = bb->add_var(Type::i32, aa++);
    v10->set_op(Operation::new_sumul_h(v10, v9, v8));

    auto v11 = bb->add_var(Type::i32, aa++);
    auto v12 = bb->add_var(Type::i32, aa++); // TODO
    v11->set_op(Operation::new_div(v11, v12, v10, v9));

    auto v13 = bb->add_var(Type::i32, aa++);
    auto v14 = bb->add_var(Type::i32, aa++); // TODO
    v13->set_op(Operation::new_udiv(v13, v14, v10, v11));

    auto v15 = bb->add_var(Type::i32, aa++);
    v15->set_op(Operation::new_shl(v15, v13, v11));

    auto v16 = bb->add_var(Type::i32, aa++);
    v16->set_op(Operation::new_shr(v16, v15, v13));

    auto v17 = bb->add_var(Type::i32, aa++);
    v17->set_op(Operation::new_sar(v17, v16, v15));

    auto v18 = bb->add_var(Type::i32, aa++);
    v18->set_op(Operation::new_or(v18, v17, v16));

    auto v19 = bb->add_var(Type::i32, aa++);
    v19->set_op(Operation::new_and(v19, v18, v17));

    auto v20 = bb->add_var(Type::i32, aa++);
    v20->set_op(Operation::new_not(v20, v19));

    auto v21 = bb->add_var(Type::i32, aa++);
    v21->set_op(Operation::new_xor(v21, v20, v19));

    auto v22 = bb->add_var(Type::i16, aa++);
    v22->set_op(Operation::new_cast(v22, v21));

    auto v23 = bb->add_var(Type::i32, aa++);
    v23->set_op(Operation::new_sltu(v23, v21, v20, v19, v18));

    auto v24 = bb->add_var(Type::i32, aa++);
    v24->set_op(Operation::new_sign_extend(v24, v22));

    auto v25 = bb->add_var(Type::i32, aa++);
    v25->set_op(Operation::new_zero_extend(v25, v22));

    auto v26 = bb->add_var(Type::i32, aa++);
    v26->set_op(Operation::new_setup_stack(v26));

    ASSERT_EQ(bb->variables.size(), aa);

    std::vector<std::string> messages;
    bool ok = ir.verify(messages);
    for (const auto &msg : messages) {
        std::cerr << msg << '\n';
    }
    ASSERT_TRUE(ok);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
