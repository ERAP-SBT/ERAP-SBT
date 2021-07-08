#include "elf.h"
#include "ir/basic_block.h"
#include "ir/ir.h"
#include "lifter/lifter.h"

#include "gtest/gtest.h"

using namespace lifter::RV64;
using reg_map = Lifter::reg_map;

TEST(SPLIT_BASIC_BLOCK_TEST, test1) {
    // define constants used for testint
    const size_t prog_start = 0;
    const size_t prog_end = 100;

    const size_t bb_start_addr = 10;
    const size_t bb_split_addr = 14;

    // initialize an ir and a lifter
    IR ir{};
    ir.setup_bb_addr_vec(prog_start, prog_end);
    for(int i = 0; i < 33; i++) {
        ir.add_static(Type::i64);
    }
    Lifter lifter{&ir};

    // create a basic block which should be splitted
    BasicBlock *block = ir.add_basic_block(bb_start_addr, "test_block_1");

    reg_map mapping{};
    // add statics
    for (int i = 1; i < mapping.size(); i++) {
        mapping.at(i) = block->add_var_from_static(i, bb_start_addr);
    }

    // instruction = addi x2, x0, 50
    RV64Inst instr = {FrvInst{FRV_ADDI, 2, 0, 0, 0, 50}, 0};
    lifter.lift_arithmetical_logical_immediate(block, instr, mapping, 12, Instruction::add, Type::i64);

    

    block->print(std::cout, &ir);
    std::cout << "\n";

    // lifter.split_basic_block(block, bb_start_addr, nullptr);
    ASSERT_TRUE(true);
}