#include "ir/ir.h"
#include "lifter/lifter.h"

#include "gtest/gtest.h"

using namespace lifter::RV64;

using reg_map = Lifter::reg_map;

class TestFloatingPointLifting : public ::testing::Test {
  public:
    IR *ir;
    Lifter *lifter;
    BasicBlock *bb;
    uint64_t virt_start_addr;
    Lifter::reg_map mapping;

    TestFloatingPointLifting() {}

    void SetUp() {
        // setup ir and lifter
        ir = new IR();
        lifter = new Lifter(ir);

        // create static mapper
        for (unsigned i = 0; i < 32; i++) {
            ir->add_static(Type::i64);
        }

        ir->add_static(Type::mt);

        for (unsigned i = 0; i < 32; i++) {
            ir->add_static(Type::f64);
        }

        ir->add_static(Type::i64);

        // create random start address
        virt_start_addr = random();
        ir->setup_bb_addr_vec(virt_start_addr, virt_start_addr + 100);
        bb = ir->add_basic_block(virt_start_addr);

        // init mapping
        mapping = {};
        for (size_t i = 0; i < COUNT_STATIC_VARS; i++) {
            mapping[i] = bb->add_var_from_static(i, virt_start_addr);
        }
    }

    void TearDown() {
        delete lifter;
        delete ir;
    }

    void verify() {
        std::vector<std::string> messages;
        bool valid = ir->verify(messages);
        for (const auto &message : messages) {
            std::cerr << message << '\n';
        }
        ASSERT_TRUE(valid) << "The IR has structural errors (see previous messages)";
    }

    void test_fp_arithmetic_lifting(const RV64Inst &instr) {
        lifter->parse_instruction(bb, instr, mapping, virt_start_addr, virt_start_addr + instr.size);
        auto *result = bb->variables[COUNT_STATIC_VARS].get();

        Type expected_op_size;
        Instruction expected_instruction;
        switch (instr.instr.mnem) {
        case FRV_FADDS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::add;
            break;
        case FRV_FADDD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::add;
            break;
        case FRV_FSUBS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::sub;
            break;
        case FRV_FSUBD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::sub;
            break;
        case FRV_FMULS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::fmul;
            break;
        case FRV_FMULD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::fmul;
            break;
        case FRV_FSQRTS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::fsqrt;
            break;
        case FRV_FSQRTD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::fsqrt;
            break;
        case FRV_FDIVS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::div;
            break;
        case FRV_FDIVD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::div;
            break;
        default:
            assert(0 && "The developer of the tests has failed!!");
        }

        ASSERT_EQ(result->type, expected_op_size) << "The result variable has the wrong type!";
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(result->info)) << "The result variable has no operation stored!";
        auto *op = std::get<std::unique_ptr<Operation>>(result->info).get();
        ASSERT_EQ(op->type, expected_instruction) << "The operation has the wrong instruction type!";
        ASSERT_EQ(op->out_vars[0], result) << "The results operations ouput isn't the result!";
        ASSERT_EQ(result, mapping[instr.instr.rd + Lifter::START_IDX_FLOATING_POINT_STATICS]) << "The result isn't written correct to the mapping!";
        ASSERT_EQ(op->in_vars[0], mapping[instr.instr.rs1 + Lifter::START_IDX_FLOATING_POINT_STATICS]) << "The first input of the instruction isn't the first source operand!";
        if (instr.instr.rs2 != 0) {
            ASSERT_EQ(op->in_vars[1], mapping[instr.instr.rs2 + Lifter::START_IDX_FLOATING_POINT_STATICS]) << "The second input of the instruction isn't the second source operand!";
        }
        ASSERT_LE(bb->variables.size(), COUNT_STATIC_VARS + 1) << "In the basic block are more variables than expected!";
        ASSERT_GE(bb->variables.size(), COUNT_STATIC_VARS + 1) << "In the basic block are less variables than expected!";
    }
};

TEST_F(TestFloatingPointLifting, test_fp_load_f) {
    // create fp load instruction: flw f3, x2, 20
    const RV64Inst instr{FrvInst{FRV_FLW, 3, 2, 0, 0, 0, 20}, 4};

    lifter->parse_instruction(bb, instr, mapping, virt_start_addr, virt_start_addr + 4);

    verify();

    // check offset immediate
    auto *offset_immediate = bb->variables[COUNT_STATIC_VARS].get();
    {
        ASSERT_TRUE(std::holds_alternative<SSAVar::ImmInfo>(offset_immediate->info));
        auto *imm_info = &std::get<SSAVar::ImmInfo>(offset_immediate->info);
        ASSERT_EQ(imm_info->val, 20) << "The immediate value is wrong!";
        ASSERT_FALSE(imm_info->binary_relative);
    }

    // check the calculation of the address
    auto *address = bb->variables[COUNT_STATIC_VARS + 1].get();
    {
        ASSERT_EQ(address->type, Type::i64) << "The address doesn't have the right type!";
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(address->info)) << "The variable with the address doesn't contain an operation!";
        auto *op = std::get<std::unique_ptr<Operation>>(address->info).get();
        ASSERT_EQ(op->type, Instruction::add) << "The operation of the address calculation has the wrong instruction type!";
        ASSERT_EQ(op->out_vars[0], address) << "The address isn't the result of the operation to calculate it!";
        ASSERT_EQ(op->in_vars[0], mapping[2]) << "The first input of the address calculation isn't the base register!";
        ASSERT_EQ(op->in_vars[1], offset_immediate) << "The second input of the address calculation isn't the offset immediate!";
    }

    // check the result variable
    {
        auto *result_variable = bb->variables[COUNT_STATIC_VARS + 2].get();
        ASSERT_EQ(result_variable, mapping[3 + Lifter::START_IDX_FLOATING_POINT_STATICS]) << "The result isn't written correctly to the mapping!";
        ASSERT_EQ(result_variable->type, Type::f32) << "The result has the wrong type!";
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(result_variable->info)) << "The result variable doesn't contain an operation!";
        auto *op = std::get<std::unique_ptr<Operation>>(result_variable->info).get();
        ASSERT_EQ(op->type, Instruction::load) << "The result variables operation doesn't have the right instruction type!";
        ASSERT_EQ(op->out_vars[0], result_variable) << "The result variable isn't the result of it's operation!";
        ASSERT_EQ(op->in_vars[0], address) << "The address isn't the first input for the operation!";
        ASSERT_EQ(op->in_vars[1], mapping[Lifter::MEM_IDX]) << "The second input of the operation isn't the memory token!";
    }
}

TEST_F(TestFloatingPointLifting, test_fp_load_d) {
    // create fp load instruction: fld f3, x2, 20
    const RV64Inst instr{FrvInst{FRV_FLD, 3, 2, 0, 0, 0, 20}, 4};

    lifter->parse_instruction(bb, instr, mapping, virt_start_addr, virt_start_addr + 4);

    verify();

    // check offset immediate
    auto *offset_immediate = bb->variables[COUNT_STATIC_VARS].get();
    {
        ASSERT_TRUE(std::holds_alternative<SSAVar::ImmInfo>(offset_immediate->info));
        auto *imm_info = &std::get<SSAVar::ImmInfo>(offset_immediate->info);
        ASSERT_EQ(imm_info->val, 20) << "The immediate value is wrong!";
        ASSERT_FALSE(imm_info->binary_relative);
    }

    // check the calculation of the address
    auto *address = bb->variables[COUNT_STATIC_VARS + 1].get();
    {
        ASSERT_EQ(address->type, Type::i64) << "The address doesn't have the right type!";
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(address->info)) << "The variable with the address doesn't contain an operation!";
        auto *op = std::get<std::unique_ptr<Operation>>(address->info).get();
        ASSERT_EQ(op->type, Instruction::add) << "The operation of the address calculation has the wrong instruction type!";
        ASSERT_EQ(op->out_vars[0], address) << "The address isn't the result of the operation to calculate it!";
        ASSERT_EQ(op->in_vars[0], mapping[2]) << "The first input of the address calculation isn't the base register!";
        ASSERT_EQ(op->in_vars[1], offset_immediate) << "The second input of the address calculation isn't the offset immediate!";
    }

    // check the result variable
    {
        auto *result_variable = bb->variables[COUNT_STATIC_VARS + 2].get();
        ASSERT_EQ(result_variable, mapping[3 + Lifter::START_IDX_FLOATING_POINT_STATICS]) << "The result isn't written correctly to the mapping!";
        ASSERT_EQ(result_variable->type, Type::f64) << "The result has the wrong type!";
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(result_variable->info)) << "The result variable doesn't contain an operation!";
        auto *op = std::get<std::unique_ptr<Operation>>(result_variable->info).get();
        ASSERT_EQ(op->type, Instruction::load) << "The result variables operation doesn't have the right instruction type!";
        ASSERT_EQ(op->out_vars[0], result_variable) << "The result variable isn't the result of it's operation!";
        ASSERT_EQ(op->in_vars[0], address) << "The address isn't the first input for the operation!";
        ASSERT_EQ(op->in_vars[1], mapping[Lifter::MEM_IDX]) << "The second input of the operation isn't the memory token!";
    }
}

TEST_F(TestFloatingPointLifting, test_fp_store) {
    // create fp store instruction: fsw f6,-16(x9)
    const unsigned base_register_id = 9;
    const unsigned source_register_id = 6;
    const int offset = -16;

    // save memory token for later comparison
    auto *prev_memory_token = mapping[Lifter::MEM_IDX];

    const RV64Inst instr{FrvInst{FRV_FSW, 0, base_register_id, source_register_id, 0, 0, offset}, 4};

    lifter->parse_instruction(bb, instr, mapping, virt_start_addr, virt_start_addr + 4);

    verify();

    // check offset immediate
    auto *offset_immediate = bb->variables[COUNT_STATIC_VARS].get();
    {
        ASSERT_TRUE(std::holds_alternative<SSAVar::ImmInfo>(offset_immediate->info));
        auto *imm_info = &std::get<SSAVar::ImmInfo>(offset_immediate->info);
        ASSERT_EQ(imm_info->val, offset) << "The immediate value is wrong!";
        ASSERT_FALSE(imm_info->binary_relative);
    }

    // check the calculation of the address
    auto *address = bb->variables[COUNT_STATIC_VARS + 1].get();
    {
        ASSERT_EQ(address->type, Type::i64) << "The address doesn't have the right type!";
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(address->info)) << "The variable with the address doesn't contain an operation!";
        auto *op = std::get<std::unique_ptr<Operation>>(address->info).get();
        ASSERT_EQ(op->type, Instruction::add) << "The operation of the address calculation has the wrong instruction type!";
        ASSERT_EQ(op->out_vars[0], address) << "The address isn't the result of the operation to calculate it!";
        ASSERT_EQ(op->in_vars[0], mapping[base_register_id]) << "The first input of the address calculation isn't the base register!";
        ASSERT_EQ(op->in_vars[1], offset_immediate) << "The second input of the address calculation isn't the offset immediate!";
    }

    // check the store operation
    {
        auto *result_memory_token = bb->variables[COUNT_STATIC_VARS + 2].get();
        ASSERT_EQ(result_memory_token, mapping[Lifter::MEM_IDX]) << "The memory token isn't written to the mapping!";
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(result_memory_token->info)) << "The result memory token doesn't contain an operation!";
        auto *op = std::get<std::unique_ptr<Operation>>(result_memory_token->info).get();
        ASSERT_EQ(op->type, Instruction::store) << "The operation isn't a store operation!";
        ASSERT_EQ(op->out_vars[0], result_memory_token) << "The result memory token isn't the output of it's operation!";
        ASSERT_EQ(op->in_vars[0], address) << "The first input of the operation isn't the store address!";
        ASSERT_EQ(op->in_vars[1], mapping[source_register_id + Lifter::START_IDX_FLOATING_POINT_STATICS]) << "The second input of the operation isn't the variable to store!";
        ASSERT_EQ(op->in_vars[2], prev_memory_token) << "The third input of the operation isn't the memory token!";
    }
}

TEST_F(TestFloatingPointLifting, test_fp_add_f) {
    // create fp add instruction: fadd.s f2, f3, f4
    const RV64Inst instr{FrvInst{FRV_FADDS, 2, 3, 4, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_add_d) {
    // create fp add instruction: fadd.d f5, f6, f7
    const RV64Inst instr{FrvInst{FRV_FADDD, 5, 6, 7, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_sub_f) {
    // create fp sub instruction: fsub.s f8, f9, f10
    const RV64Inst instr{FrvInst{FRV_FSUBS, 8, 9, 10, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_sub_d) {
    // create fp sub instruction: fsub.d f11, f12, f13
    const RV64Inst instr{FrvInst{FRV_FSUBD, 11, 12, 13, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

// TODO: Fails due to static mapper with Type::f64, but combined with Type::f32 instruction... Change static typed static mapper? Convert types everytime? Cast?
TEST_F(TestFloatingPointLifting, test_fp_mul_f) {
    // create fp mul instruction: fmul.s f12, f3, f4
    const RV64Inst instr{FrvInst{FRV_FMULS, 12, 3, 4, 0, 0, 0}, 4};
    // test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_mul_d) {
    // create fp mul instruction: fmul.d f24, f6, f4
    const RV64Inst instr{FrvInst{FRV_FMULD, 24, 6, 4, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

// TODO: Fails due to static mapper with Type::f64, but combined with Type::f32 instruction... Change static typed static mapper? Convert types everytime? Cast?
TEST_F(TestFloatingPointLifting, test_fp_div_s) {
    // create fp div instruction: fdiv.s f5, f30, f6
    const RV64Inst instr{FrvInst{FRV_FDIVS, 5, 30, 6, 0, 0, 0}, 4};
    // test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_div_d) {
    // create fp div instruction: fdiv.d f2, f28, f14
    const RV64Inst instr{FrvInst{FRV_FDIVD, 2, 28, 14, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

// TODO: Fails due to static mapper with Type::f64, but combined with Type::f32 instruction... Change static typed static mapper? Convert types everytime? Cast?
TEST_F(TestFloatingPointLifting, test_fp_sqrt_s) {
    // create fp sqrt instruction: fsqrt.s f5, f25
    const RV64Inst instr{FrvInst{FRV_FSQRTS, 5, 25, 0, 0, 0, 0}, 4};
    // test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_sqrt_d) {
    // create fp sqrt instruction: fsqrt.d f3, f9
    const RV64Inst instr{FrvInst{FRV_FSQRTD, 3, 9, 0, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}