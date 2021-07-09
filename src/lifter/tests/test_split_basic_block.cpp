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
    RV64Inst instructions[3] = {RV64Inst{FrvInst{FRV_ADDI, 2, 0, 0, 0, 0, 50}, 0}, RV64Inst{FrvInst{FRV_ANDI, 3, 2, 0, 0, 0, 16}, 0}, RV64Inst{FrvInst{FRV_SUB, 2, 3, 2, 0, 0, 0}, 0}};

    std::vector<int> count_var_per_instr{};
    {
        int curr_ip = bb_start_addr;
        size_t previous_size = count_static_vars;
        for (RV64Inst &instr : instructions) {
            lifter.parse_instruction(instr, block, mapping, curr_ip, curr_ip + 4);
            count_var_per_instr.push_back((int)(block->variables.size() - previous_size));
            previous_size = block->variables.size();
            curr_ip += 4;
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
    // define constants used for test
    const size_t prog_start = 0;
    const size_t prog_end = 100;

    const size_t bb_start_addr = 8;
    // the instruction is in the first basic block, the remainder is in the second one
    const size_t bb_split_addr = 24;
    const size_t bb_end_addr = 36;

    IR ir{};
    ir.setup_bb_addr_vec(prog_start, prog_end);

    // add static mapper
    for (int i = 0; i < 33; i++) {
        ir.add_static(Type::i64);
    }

    Lifter lifter{&ir};

    // create a basic block which should be splitted
    BasicBlock *block = ir.add_basic_block(bb_start_addr, "test_block_1");

    block->set_virt_end_addr(bb_end_addr);

    reg_map mapping{};

    // add statics
    for (unsigned long i = 1; i < mapping.size(); i++) {
        mapping.at(i) = block->add_var_from_static(i, bb_start_addr);
    }

    const size_t count_static_vars = block->variables.size();

    // create instructions
    RV64Inst instructions[11];

    // addi x2, x0, 69
    instructions[0] = RV64Inst{FrvInst{FRV_ADDI, 2, 0, 0, 0, 0, 69}, 0};
    // addi x3, x0, 42
    instructions[1] = RV64Inst{FrvInst{FRV_ADDI, 3, 0, 0, 0, 0, 42}, 0};
    // addi x4, x0, 1337
    instructions[2] = RV64Inst{FrvInst{FRV_ADDI, 4, 0, 0, 0, 0, 1337}, 0};
    // addi x5, x0, 420
    instructions[3] = RV64Inst{FrvInst{FRV_ADDI, 5, 0, 0, 0, 0, 420}, 0};

    // xor x6, x2, x3
    instructions[4] = RV64Inst{FrvInst{FRV_XOR, 6, 2, 3, 0, 0, 0}, 0};
    // xor x7, x4, x5
    instructions[5] = RV64Inst{FrvInst{FRV_XOR, 7, 4, 5, 0, 0, 0}, 0};

    // add x3, x3, x2
    instructions[6] = RV64Inst{FrvInst{FRV_ADD, 3, 3, 2, 0, 0, 0}, 0};
    // add x5, x5, x4
    instructions[7] = RV64Inst{FrvInst{FRV_ADD, 5, 5, 4, 0, 0, 0}, 0};

    // and x6, x6, x3
    instructions[8] = RV64Inst{FrvInst{FRV_AND, 6, 6, 3, 0, 0, 0}, 0};
    // and x7, x7, x5
    instructions[9] = RV64Inst{FrvInst{FRV_AND, 7, 7, 5, 0, 0, 0}, 0};

    // sub x7, x7, x6
    instructions[10] = RV64Inst{FrvInst{FRV_SUB, 7, 7, 6, 0, 0, 0}, 0};

    uint64_t count_vars_before = 0;
    uint64_t count_vars_after = 0;
    {
        uint64_t curr_ip = bb_start_addr;
        for (RV64Inst &instr : instructions) {
            lifter.parse_instruction(instr, block, mapping, curr_ip, curr_ip + 4);
            if (curr_ip >= bb_split_addr) {
                count_vars_after = block->variables.size() - count_static_vars - count_vars_before;
            } else {
                count_vars_before = block->variables.size() - count_static_vars;
            }
            curr_ip += 4;
        }
    }

    std::cout << count_vars_before << "\n";
    std::cout << count_vars_after << "\n";
}