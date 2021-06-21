#include "ir/ir.h"
#include "lifter/lifter.h"

#include "gtest/gtest.h"

using namespace lifter::RV64;
namespace lifter_test {

class TestArithmeticalLogicalLifting : public ::testing::Test {
  public:
    IR *ir;
    Lifter *lifter;
    BasicBlock *bb;
    uint64_t virt_start_addr;
    Lifter::reg_map mapping;

    TestArithmeticalLogicalLifting() {}

    void SetUp() {
        // ir = IR{};
        ir = new IR();
        lifter = new Lifter(ir);
        virt_start_addr = random();
        bb = ir->add_basic_block(virt_start_addr);
        mapping = Lifter::reg_map{};
        for (size_t i = 0; i < mapping.size(); i++) {
            mapping.at(i) = bb->add_var(Type::i64, 1, i);
        }
    }
};

TEST_F(TestArithmeticalLogicalLifting, test_lift_arithmetical_logical_logical1) {
    // instruction is: add x2, x3, x4
    RV64Inst instr{FrvInst{FRV_ADD, 2, 3, 4, 0, 0, 0}, 4};
    size_t prev_size_variables = bb->variables.size();
    lifter->lift_arithmetical_logical(bb, instr, mapping, 0x69420, Instruction::add, Type::i64);
    ASSERT_TRUE(bb->variables.size() > prev_size_variables) << "There have to be added some ssa variables!";
}

TEST_F(TestArithmeticalLogicalLifting, test_lift_arithmetical_logical_logical2) {
    // instruction is: add x2, x3, x4
    RV64Inst instr{FrvInst{FRV_ADD, 2, 3, 4, 0, 0, 0}, 4};

    // store some variables in order to test if they were changed correctly
    size_t prev_size_variables = bb->variables.size();
    size_t last_ssa_id = bb->variables.back().get()->id;

    // lift the instruction
    lifter->lift_arithmetical_logical(bb, instr, mapping, 0x69420, Instruction::add, Type::i64);

    ASSERT_TRUE(bb->variables.size() > prev_size_variables) << "There have to be added some ssa variables.";

    SSAVar *last_var = bb->variables.back().get();
    ASSERT_EQ(last_ssa_id + 1, last_var->id) << "The id of the result ssavar is not set correctly.";

    ASSERT_FALSE(last_var->from_static) << "The result ssavar is not from static.";
    ASSERT_EQ(last_var->type, Type::i64) << "The result ssavar has the wrong type.";

    ASSERT_EQ(last_var->info.index(), 2) << "The info is not set correctly: A operation should be stored.";
    Operation *operation = std::get<std::unique_ptr<Operation>>(last_var->info).get();

    ASSERT_EQ(operation->type, Instruction::add) << "The operation has the wrong instruction type.";
    ASSERT_FALSE(operation->const_evaluable) << "The operation must not be const evaluable.";

    // count the non-null input vars
    int count_input_vars = 0;
    for (size_t i = 0; i < operation->in_vars.size(); i++) {
        if (operation->in_vars.at(i) != nullptr) {
            count_input_vars++;
        }
    }
    ASSERT_FALSE(count_input_vars < 2) << "There are too few input vars.";
    ASSERT_FALSE(count_input_vars > 2) << "There are too much input vars.";

    ASSERT_EQ(mapping.at(3), operation->in_vars.at(0)) << "The first input var is wrong.";
    ASSERT_EQ(mapping.at(4), operation->in_vars.at(1)) << "The second input var is wrong.";

    // count the non-null output vars
    int count_output_vars = 0;
    for (size_t i = 0; i < operation->out_vars.size(); i++) {
        if (operation->out_vars.at(i) != nullptr) {
            count_output_vars++;
        }
    }
    ASSERT_FALSE(count_output_vars < 1) << "There are too few output vars.";
    ASSERT_FALSE(count_output_vars > 1) << "There are too much output vars.";

    ASSERT_EQ(last_var, operation->out_vars.at(0)) << "The output var of the operation is not the result ssavar.";
    ASSERT_EQ(mapping.at(2), last_var) << "The result ssavar is not written back to the mapping correctly.";
}

TEST_F(TestArithmeticalLogicalLifting, test_lift_arithmetical_logical_logical3) {
    // instruction is: add x6, x8, x10
    RV64Inst instr{FrvInst{FRV_SUB, 6, 8, 10, 0, 0, 0}, 4};

    // store some variables in order to test if they were changed correctly
    size_t prev_size_variables = bb->variables.size();
    size_t last_ssa_id = bb->variables.back().get()->id;

    // lift the instruction
    lifter->lift_arithmetical_logical(bb, instr, mapping, 0x69420, Instruction::sub, Type::i64);

    ASSERT_TRUE(bb->variables.size() > prev_size_variables) << "There have to be added some ssa variables.";

    SSAVar *last_var = bb->variables.back().get();
    ASSERT_EQ(last_ssa_id + 1, last_var->id) << "The id of the result ssavar is not set correctly.";

    ASSERT_FALSE(last_var->from_static) << "The result ssavar is not from static.";
    ASSERT_EQ(last_var->type, Type::i64) << "The result ssavar has the wrong type.";

    ASSERT_EQ(last_var->info.index(), 2) << "The info is not set correctly: A operation should be stored.";
    Operation *operation = std::get<std::unique_ptr<Operation>>(last_var->info).get();

    ASSERT_EQ(operation->type, Instruction::sub) << "The operation has the wrong instruction type.";
    ASSERT_FALSE(operation->const_evaluable) << "The operation must not be const evaluable.";

    // count the non-null input vars
    int count_input_vars = 0;
    for (size_t i = 0; i < operation->in_vars.size(); i++) {
        if (operation->in_vars.at(i) != nullptr) {
            count_input_vars++;
        }
    }
    ASSERT_FALSE(count_input_vars < 2) << "There are too few input vars.";
    ASSERT_FALSE(count_input_vars > 2) << "There are too much input vars.";

    ASSERT_EQ(mapping.at(8), operation->in_vars.at(0)) << "The first input var is wrong.";
    ASSERT_EQ(mapping.at(10), operation->in_vars.at(1)) << "The second input var is wrong.";

    // count the non-null output vars
    int count_output_vars = 0;
    for (size_t i = 0; i < operation->out_vars.size(); i++) {
        if (operation->out_vars.at(i) != nullptr) {
            count_output_vars++;
        }
    }
    ASSERT_FALSE(count_output_vars < 1) << "There are too few output vars.";
    ASSERT_FALSE(count_output_vars > 1) << "There are too much output vars.";

    ASSERT_EQ(last_var, operation->out_vars.at(0)) << "The output var of the operation is not the result ssavar.";
    ASSERT_EQ(mapping.at(6), last_var) << "The result ssavar is not written back to the mapping correctly.";
}

TEST_F(TestArithmeticalLogicalLifting, test_lift_arithmetical_logical_immediate_1) {
    // the instruction is: xor x9, x1, 15
    int32_t immediate = 15;
    RV64Inst instr{FrvInst{FRV_XORI, 9, 1, 0, 0, 0, immediate}, 4};

    // store some variables in order to test if they were changed correctly
    size_t prev_size_variables = bb->variables.size();
    size_t last_ssa_id = bb->variables.back().get()->id;

    lifter->lift_arithmetical_logical_immediate(bb, instr, mapping, 0x13370, Instruction::_xor, Type::i64);

    ASSERT_TRUE(bb->variables.size() > prev_size_variables) << "There have to be added some ssa variables.";

    SSAVar *last_var = bb->variables.back().get();
    ASSERT_EQ(last_ssa_id + 2, last_var->id) << "The id of the result ssavar is not set correctly.";

    ASSERT_FALSE(last_var->from_static) << "The result ssavar is not from static.";
    ASSERT_EQ(last_var->type, Type::i64) << "The result ssavar has the wrong type.";

    ASSERT_EQ(last_var->info.index(), 2) << "The info is not set correctly: A operation should be stored.";
    Operation *operation = std::get<std::unique_ptr<Operation>>(last_var->info).get();

    ASSERT_EQ(operation->type, Instruction::_xor) << "The operation has the wrong instruction type.";
    ASSERT_FALSE(operation->const_evaluable) << "The operation must not be const evaluable.";

    // count the non-null input vars
    int count_input_vars = 0;
    for (size_t i = 0; i < operation->in_vars.size(); i++) {
        if (operation->in_vars.at(i) != nullptr) {
            count_input_vars++;
        }
    }
    ASSERT_FALSE(count_input_vars < 2) << "There are too few input vars.";
    ASSERT_FALSE(count_input_vars > 2) << "There are too much input vars.";

    ASSERT_EQ(mapping.at(1), operation->in_vars.at(0)) << "The first input var is wrong.";

    SSAVar *immediate_input_var = operation->in_vars.at(1);

    ASSERT_EQ(operation->in_vars.at(1)->type, Type::imm) << "The second operand should habe the type immediate.";
    ASSERT_EQ(last_ssa_id + 1, immediate_input_var->id) << "The id of the immediate ssavar is not set correctly.";
    ASSERT_TRUE(operation->in_vars.at(1)->const_evaluable) << "The second operand (the immediate) should be const evaluable.";
    ASSERT_FALSE(immediate_input_var->from_static) << "The immediate ssavar should not be marked as from static.";

    ASSERT_EQ(immediate_input_var->info.index(), 1) << "The info of the immediate ssavar should contain an immediate value.";
    ASSERT_EQ(std::get<int64_t>(immediate_input_var->info), immediate) << "The stored immediate in the variable is not correct.";

    // count the non-null output vars
    int count_output_vars = 0;
    for (size_t i = 0; i < operation->out_vars.size(); i++) {
        if (operation->out_vars.at(i) != nullptr) {
            count_output_vars++;
        }
    }
    ASSERT_FALSE(count_output_vars < 1) << "There are too few output vars.";
    ASSERT_FALSE(count_output_vars > 1) << "There are too much output vars.";

    ASSERT_EQ(last_var, operation->out_vars.at(0)) << "The output var of the operation is not the result ssavar.";
    ASSERT_EQ(mapping.at(9), last_var) << "The result ssavar is not written back to the mapping correctly.";
}

} // namespace lifter_test

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
