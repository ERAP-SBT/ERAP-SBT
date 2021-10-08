#include "elf.h"
#include "ir/basic_block.h"
#include "ir/ir.h"
#include "lifter/lifter.h"

#include "gtest/gtest.h"
#include <string>

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

    Lifter lifter{&ir};

    lifter.add_statics();

    // create a basic block which should be splitted
    BasicBlock *block = ir.add_basic_block(bb_start_addr, "test_block_1");

    block->set_virt_end_addr(20);

    reg_map mapping{};

    // add statics
    for (unsigned long i = 1; i < lifter.count_used_static_vars; i++) {
        mapping[i] = block->add_var_from_static(i, bb_start_addr);
    }

    ASSERT_EQ(block->variables.size(), COUNT_STATIC_VARS - 1) << "The amount of statics variables is not as expected!";

    // lifting some instructions to generate some variables
    // instructions = (addi x2, x0, 50), (andi x3, x2, 16), (sub x2, x3, x2)
    RV64Inst instructions[3] = {RV64Inst{FrvInst{FRV_ADDI, 2, 0, 0, 0, 0, 50}, 0}, RV64Inst{FrvInst{FRV_ANDI, 3, 2, 0, 0, 0, 16}, 0}, RV64Inst{FrvInst{FRV_SUB, 2, 3, 2, 0, 0, 0}, 0}};

    std::vector<int> count_var_per_instr{};
    {
        int curr_ip = bb_start_addr;
        size_t previous_size = COUNT_STATIC_VARS - 1;
        for (RV64Inst &instr : instructions) {
            lifter.parse_instruction(block, instr, mapping, curr_ip, curr_ip + 4);
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
    ASSERT_EQ(block->successors.size(), 1) << "The first basic block must have exactly one sucessors!";
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
        ASSERT_EQ(second_block->inputs.size(), cfop.target_input_count()) << "The second basic block must have as much inputs as the cfop has target_inputs!";
    }

    // test for correct variable division
    {
        // first basic block
        {
            int count_non_static_vars = first_block->variables.size() - COUNT_STATIC_VARS + 1;
            ASSERT_GE(count_non_static_vars, count_var_per_instr[0]) << "The variables before the split address must be in the first basic block!";
            ASSERT_LE(count_non_static_vars, count_var_per_instr[0]) << "The variables after the split address must not be in the first basic block!";
        }

        // second basic block
        {
            int count_non_static_vars = second_block->variables.size() - COUNT_STATIC_VARS + 1;
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

    // constants for some dummy blocks (used as jump targets)
    const size_t count_dummy_blocks = 2;
    const size_t dummy_block_addrs[count_dummy_blocks] = {76, 80};

    IR ir{};
    ir.setup_bb_addr_vec(prog_start, prog_end);

    Lifter lifter{&ir};

    lifter.add_statics();

    // create a basic block which should be splitted
    BasicBlock *block = ir.add_basic_block(bb_start_addr, "test_block_1");

    // create dummy basic blocks which are only used as target for cfops
    BasicBlock *dummy_blocks[count_dummy_blocks];
    for (unsigned long i = 0; i < count_dummy_blocks; i++) {
        std::stringstream str;
        str << "dummy_block" << i;
        dummy_blocks[i] = ir.add_basic_block(dummy_block_addrs[i], str.str());
    }

    block->set_virt_end_addr(bb_end_addr);

    reg_map mapping{};

    // add statics
    for (unsigned long i = 1; i < lifter.count_used_static_vars; i++) {
        mapping[i] = block->add_var_from_static(i, bb_start_addr);
    }

    ASSERT_EQ(block->variables.size(), COUNT_STATIC_VARS - 1) << "The amount of statics variables is not as expected!";

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

    // store which ssavar belongs to which part of the basic block
    std::vector<SSAVar *> vars_before{};
    std::vector<SSAVar *> vars_after{};

    uint64_t curr_ip = bb_start_addr;
    unsigned long previous_count_var = COUNT_STATIC_VARS - 1;
    for (RV64Inst &instr : instructions) {
        // lift the instruction
        lifter.parse_instruction(block, instr, mapping, curr_ip, curr_ip + 4);

        // store which variable should belong to which part of the basic block (before or after the split address)
        if (curr_ip >= bb_split_addr) {
            for (unsigned long i = previous_count_var; i < block->variables.size(); i++) {
                vars_after.push_back(block->variables[i].get());
            }
        } else {
            for (unsigned long i = previous_count_var; i < block->variables.size(); i++) {
                vars_before.push_back(block->variables[i].get());
            }
        }
        previous_count_var = block->variables.size();
        curr_ip += 4;
    }

    std::vector<SSAVar *> vars_from_cfop{};

    // add a branch cfop (<=> creates 2 cfops in the basic block)
    {
        // beq x7, x0, off (the offset is so that the jump goes to the basic block at address dummy_bb_addr1)
        RV64Inst instr = {{FRV_BEQ, 0, 7, 0, 0, 0, (int32_t)(dummy_block_addrs[0] - curr_ip)}, 0};
        lifter.parse_instruction(block, instr, mapping, curr_ip, curr_ip + 4);
        for (unsigned long i = previous_count_var; i < block->variables.size(); i++) {
            vars_after.push_back(block->variables[i].get());
        }
        previous_count_var = block->variables.size();
    }

    std::vector<CfOp *> control_flow_ops{};

    // set cfop inputs and such stuff
    for (unsigned long i = 0; i < block->control_flow_ops.size(); i++) {
        CfOp &cfop = block->control_flow_ops[i];
        cfop.set_target(dummy_blocks[i]);

        block->successors.push_back(dummy_blocks[i]);
        dummy_blocks[i]->predecessors.push_back(block);

        for (unsigned long i = 0; i < lifter.count_used_static_vars; i++) {
            SSAVar *var = mapping[i];
            if (var != nullptr) {
                cfop.add_target_input(var, i);
                std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = i;
            }
        }

        control_flow_ops.push_back(&cfop);
    }
    lifter.split_basic_block(block, bb_split_addr, nullptr);

    // test that the ir is vaild after splitting
    verify(&ir);

    // test for correct splitting
    BasicBlock *first_block = block;

    ASSERT_EQ(first_block->successors.size(), 1) << "The first basic block must have only one successors, the second block!";
    BasicBlock *second_block = first_block->successors[0];
    ASSERT_NE(first_block, second_block) << "The first and the second basic block cannot be the same!";
    ASSERT_EQ(second_block->predecessors.size(), 1) << "The second basic block must have only one predecessor, the first block!";
    ASSERT_EQ(first_block, second_block->predecessors[0]) << "The second basic blocks predecessor must be the first basic block!";

    // first basic block validation
    {
        unsigned long count_non_static = first_block->variables.size() - COUNT_STATIC_VARS + 1;
        ASSERT_GE(count_non_static, vars_before.size()) << "The first basic block has too few variables!";
        ASSERT_LE(count_non_static, vars_before.size()) << "The first basic block has to much variables!";

        // test that no variable must be in the second basic block
        for (std::unique_ptr<SSAVar> &ptr : first_block->variables) {
            SSAVar *var = ptr.get();

            // skip statics
            if (var->is_static()) {
                continue;
            }
            ASSERT_NE(std::find(vars_before.begin(), vars_before.end(), var), vars_before.end()) << "The first basic block contains a wrong variable!";
        }
    }

    // second basic block validation
    {
        unsigned long count_non_static = second_block->variables.size() - COUNT_STATIC_VARS + 1;
        ASSERT_GE(count_non_static, vars_after.size() + vars_from_cfop.size()) << "The second basic block has too few variables!";
        ASSERT_LE(count_non_static, vars_after.size() + vars_from_cfop.size()) << "The second basic block has too much variables!";

        // test that no variable must be in the first basic block
        for (std::unique_ptr<SSAVar> &ptr : second_block->variables) {
            SSAVar *var = ptr.get();

            // skip statics
            if (var->is_static()) {
                continue;
            }

            bool is_in_vars_after = std::find(vars_after.begin(), vars_after.end(), var) != vars_after.end();
            bool is_in_cfop_vars = std::find(vars_from_cfop.begin(), vars_from_cfop.end(), var) != vars_from_cfop.end();
            ASSERT_TRUE(is_in_vars_after || is_in_cfop_vars) << "The second basic block contains a wrong variable!";
        }

        ASSERT_EQ(second_block->control_flow_ops.size(), control_flow_ops.size()) << "The second basic block must have the cfops of the original basic block!";

        // test that all cfops of the second_block are those which were in the original basic block
        for (CfOp &cfop : second_block->control_flow_ops) {
            ASSERT_NE(std::find(control_flow_ops.begin(), control_flow_ops.end(), &cfop), control_flow_ops.end()) << "The second basic block contains a cfop which should not be there!";
        }
    }

    // test for correct cfop between first and second basic block
    {
        ASSERT_EQ(first_block->control_flow_ops.size(), 1) << "The first basic block must have only one cfop!"; //, a jump to the second basic block!";
        CfOp &jump = first_block->control_flow_ops[0];
        ASSERT_EQ(jump.type, CFCInstruction::jump) << "Between first and second basic block has to be a jump!";
        ASSERT_EQ(jump.source, first_block) << "The source of the jump between first and second basic block has to be the first basic block!";
        ASSERT_EQ(jump.target(), second_block) << "The source of the jump between first and second basic block has to be the second basic block!";
    }

    // ir.print(std::cout);
}
