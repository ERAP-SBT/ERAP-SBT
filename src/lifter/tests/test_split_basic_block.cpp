#include "elf.h"
#include "ir/basic_block.h"
#include "ir/ir.h"
#include "lifter/lifter.h"

#include "gtest/gtest.h"

using namespace lifter::RV64;
using reg_map = Lifter::reg_map;

void verify(IR *ir) {
    std::vector<std::string> messages;
    bool valid = ir->verify(messages);
    for (const auto &message : messages) {
        std::cerr << message << '\n';
    }
    ASSERT_TRUE(valid) << "The IR has structural errors (see error log)";
}

TEST(SPLIT_BASIC_BLOCK_TEST, test_small) {
    // define constants used for test
    const size_t prog_start = 0;
    const size_t prog_end = 100;

    const size_t bb_start_addr = 8;
    // the instruction is in the first basic block, the remainder is in the second one
    const size_t bb_split_addr = 12;

    // initialize an ir and a lifter
    IR ir{};
    ir.setup_bb_addr_vec(prog_start, prog_end);

    // add static mapper
    for (int i = 0; i < 33; i++) {
        ir.add_static(Type::i64);
    }

    Lifter lifter{&ir};

    // create a basic block which should be splitted
    BasicBlock *block = ir.add_basic_block(bb_start_addr, "test_block_1");

    block->set_virt_end_addr(20);

    reg_map mapping{};

    // add statics
    for (unsigned long i = 1; i < mapping.size(); i++) {
        mapping.at(i) = block->add_var_from_static(i, bb_start_addr);
    }

    const size_t count_static_vars = block->variables.size();

    // lifting some instructions to generate some variables
    // instructions = (addi x2, x0, 50), (andi x3, x2, 16), (sub x2, x3, x2)
    RV64Inst instrcutions[3] = {RV64Inst{FrvInst{FRV_ADDI, 2, 0, 0, 0, 0, 50}, 0}, RV64Inst{FrvInst{FRV_ANDI, 3, 2, 0, 0, 0, 16}, 0}, RV64Inst{FrvInst{FRV_SUB, 2, 3, 2, 0, 0, 0}, 0}};

    std::vector<int> count_var_per_instr{};
    {
        int currr_addr = bb_start_addr;
        size_t previous_size = count_static_vars;
        for (RV64Inst &instr : instrcutions) {
            lifter.parse_instruction(instr, block, mapping, currr_addr, currr_addr + 4);
            count_var_per_instr.push_back((int)(block->variables.size() - previous_size));
            previous_size = block->variables.size();
            currr_addr += 4;
        }
    }
    // split the block
    lifter.split_basic_block(block, bb_split_addr, nullptr);

    // verify that the ir is correct after splitting
    verify(&ir);

    // get second basic block and test for correct sucessor/predecessors
    BasicBlock *first_block = block;
    ASSERT_TRUE(block->successors.size()) << "The first basic block must have any sucessors!";
    BasicBlock *second_block = block->successors[0];

    ASSERT_NE(first_block, second_block) << "The first and the second basic block cannot be the same!";

    ASSERT_TRUE(second_block->predecessors.size()) << "The second basic block must have any predecessors!";
    ASSERT_EQ(first_block, second_block->predecessors[0]) << "The first basic block must be a predecessor of the second one!";

    // test for an correct jump both basic blocks
    {
        ASSERT_EQ(first_block->control_flow_ops.size(), 1) << "The first basic block should have exactly one control flow operation!";
        CfOp cfop = first_block->control_flow_ops[0];
        ASSERT_EQ(cfop.type, CFCInstruction::jump) << "The cfop between both basic blocks must be an jump!";
        ASSERT_EQ(cfop.source, first_block) << "The first basic block must be the source of the cfop!";
        ASSERT_EQ(cfop.target(), second_block) << "The second basic block must be the target of the cfop!";
        ASSERT_EQ(second_block->inputs.size(), cfop.target_inputs().size()) << "The second basic block must have as much inputs as the cfop has target_inputs!";
    }

    // test for correct variable division
    {
        // first basic block
        {
            std::cout << count_var_per_instr[0] << "\n";
            int count_non_static_vars = first_block->variables.size() - count_static_vars;
            ASSERT_GE(count_non_static_vars, count_var_per_instr[0]) << "The variables before the split address must be in the first basic block!";
            ASSERT_LE(count_non_static_vars, count_var_per_instr[0]) << "The variables after the split address must not be in the first basic block!";
        }

        // second basic block
        {
            int count_non_static_vars = second_block->variables.size() - count_static_vars;
            ASSERT_GE(count_non_static_vars, count_var_per_instr[1] + count_var_per_instr[2]) << "The variables after the split address must be in the second basic block!";
            ASSERT_LE(count_non_static_vars, count_var_per_instr[1] + count_var_per_instr[2]) << "The variables before the split address must not be in the second basic block!";
        }
    }
}

TEST(SPLIT_BASIC_BLOCK_TEST, test_big) {

}