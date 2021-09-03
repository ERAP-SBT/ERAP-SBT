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
        // save pointers for later comparison, prevent overriding in the mapping
        SSAVar *input_one = mapping[instr.instr.rs1 + Lifter::START_IDX_FLOATING_POINT_STATICS];
        SSAVar *input_two = mapping[instr.instr.rs2 + Lifter::START_IDX_FLOATING_POINT_STATICS];
        SSAVar *input_three = mapping[instr.instr.rs3 + Lifter::START_IDX_FLOATING_POINT_STATICS];

        lifter->parse_instruction(bb, instr, mapping, virt_start_addr, virt_start_addr + instr.size);

        verify();

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
            expected_instruction = Instruction::fdiv;
            break;
        case FRV_FDIVD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::fdiv;
            break;
        case FRV_FMINS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::fmin;
            break;
        case FRV_FMIND:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::fmin;
            break;
        case FRV_FMAXS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::fmax;
            break;
        case FRV_FMAXD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::fmax;
            break;
        case FRV_FMADDS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::ffmadd;
            break;
        case FRV_FMADDD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::ffmadd;
            break;
        case FRV_FMSUBS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::ffmsub;
            break;
        case FRV_FMSUBD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::ffmsub;
            break;
        case FRV_FNMADDS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::ffnmsub;
            break;
        case FRV_FNMADDD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::ffnmsub;
            break;
        case FRV_FNMSUBS:
            expected_op_size = Type::f32;
            expected_instruction = Instruction::ffnmadd;
            break;
        case FRV_FNMSUBD:
            expected_op_size = Type::f64;
            expected_instruction = Instruction::ffnmadd;
            break;

        default:
            assert(0 && "The developer of the tests has failed!!");
        }

        unsigned count_scanned_variables = 0;

        // if single precision type expect casts
        if (expected_op_size == Type::f32) {
            count_scanned_variables++;
            SSAVar *casted_input_one = bb->variables[COUNT_STATIC_VARS].get();
            {
                ASSERT_EQ(casted_input_one->type, Type::f32) << "The casted first input has the wrong type!";
                ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(casted_input_one->info)) << "The casted first input has no operation!";
                auto *cast_op = std::get<std::unique_ptr<Operation>>(casted_input_one->info).get();
                ASSERT_EQ(cast_op->type, Instruction::cast) << "The operation of the casted first input is not a cast!";
                ASSERT_EQ(cast_op->out_vars[0], casted_input_one) << "The result of the first cast isn't the casted first input!";
                ASSERT_EQ(cast_op->in_vars[0], input_one) << "The input of the first cast operation isn't the first input!";
                input_one = casted_input_one;
            }
            if (instr.instr.rs2 != 0) {
                count_scanned_variables++;
                SSAVar *casted_input_two = bb->variables[COUNT_STATIC_VARS + 1].get();
                ASSERT_EQ(casted_input_two->type, Type::f32) << "The casted second input has the wrong type!";
                ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(casted_input_two->info)) << "The casted second input has no operation!";
                auto *cast_op = std::get<std::unique_ptr<Operation>>(casted_input_two->info).get();
                ASSERT_EQ(cast_op->type, Instruction::cast) << "The operation of the casted second input is not a cast!";
                ASSERT_EQ(cast_op->out_vars[0], casted_input_two) << "The result of the second cast isn't the casted second input!";
                ASSERT_EQ(cast_op->in_vars[0], input_two) << "The input of the second cast operation isn't the second input!";
                input_two = casted_input_two;
            }
            if (instr.instr.rs3 != 0) {
                count_scanned_variables++;
                SSAVar *casted_input_three = bb->variables[COUNT_STATIC_VARS + 2].get();
                ASSERT_EQ(casted_input_three->type, Type::f32) << "The casted third input has the wrong type!";
                ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(casted_input_three->info)) << "The casted third input has no operation!";
                auto *cast_op = std::get<std::unique_ptr<Operation>>(casted_input_three->info).get();
                ASSERT_EQ(cast_op->type, Instruction::cast) << "The operation of the casted third input is not a cast!";
                ASSERT_EQ(cast_op->out_vars[0], casted_input_three) << "The result of the third cast isn't the casted third input!";
                ASSERT_EQ(cast_op->in_vars[0], input_three) << "The input of the third cast operation isn't the third input!";
                input_three = casted_input_three;
            }
        }

        auto *result = bb->variables[COUNT_STATIC_VARS + count_scanned_variables].get();

        ASSERT_EQ(result->type, expected_op_size) << "The result variable has the wrong type!";
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<Operation>>(result->info)) << "The result variable has no operation stored!";
        auto *op = std::get<std::unique_ptr<Operation>>(result->info).get();
        ASSERT_EQ(op->type, expected_instruction) << "The operation has the wrong instruction type!";
        ASSERT_EQ(op->out_vars[0], result) << "The results operations ouput isn't the result!";
        ASSERT_EQ(result, mapping[instr.instr.rd + Lifter::START_IDX_FLOATING_POINT_STATICS]) << "The result isn't written correct to the mapping!";
        ASSERT_EQ(op->in_vars[0], input_one) << "The first input of the instruction isn't the first source operand!";
        if (instr.instr.rs2 != 0) {
            ASSERT_EQ(op->in_vars[1], input_two) << "The second input of the instruction isn't the second source operand!";
        }
        if (instr.instr.rs3 != 0) {
            ASSERT_EQ(op->in_vars[2], input_three) << "The third input of the instruction isn't the third source operand!";
        }

        count_scanned_variables++;

        ASSERT_LE(bb->variables.size(), COUNT_STATIC_VARS + count_scanned_variables) << "In the basic block are more variables than expected!";
        ASSERT_GE(bb->variables.size(), COUNT_STATIC_VARS + count_scanned_variables) << "In the basic block are less variables than expected!";
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

TEST_F(TestFloatingPointLifting, test_fp_mul_f) {
    // create fp mul instruction: fmul.s f12, f3, f4
    const RV64Inst instr{FrvInst{FRV_FMULS, 12, 3, 4, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_mul_d) {
    // create fp mul instruction: fmul.d f24, f6, f4
    const RV64Inst instr{FrvInst{FRV_FMULD, 24, 6, 4, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_div_s) {
    // create fp div instruction: fdiv.s f5, f30, f6
    const RV64Inst instr{FrvInst{FRV_FDIVS, 5, 30, 6, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_div_d) {
    // create fp div instruction: fdiv.d f2, f28, f14
    const RV64Inst instr{FrvInst{FRV_FDIVD, 2, 28, 14, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_sqrt_s) {
    // create fp sqrt instruction: fsqrt.s f5, f25
    const RV64Inst instr{FrvInst{FRV_FSQRTS, 5, 25, 0, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_sqrt_d) {
    // create fp sqrt instruction: fsqrt.d f3, f9
    const RV64Inst instr{FrvInst{FRV_FSQRTD, 3, 9, 0, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_min_f) {
    // create fp sqrt instruction: fmin.s f8, f3, f5
    const RV64Inst instr{FrvInst{FRV_FMINS, 8, 3, 5, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_min_d) {
    // create fp sqrt instruction: fmin.d f23, f17, f9
    const RV64Inst instr{FrvInst{FRV_FMIND, 23, 17, 9, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_max_f) {
    // create fp sqrt instruction: fmax.s f1, f5, f2
    const RV64Inst instr{FrvInst{FRV_FMAXS, 1, 5, 2, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_max_d) {
    // create fp sqrt instruction: fmax.d f4, f2, f24
    const RV64Inst instr{FrvInst{FRV_FMAXD, 4, 2, 24, 0, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_fmadd_f) {
    // create instruction: FMADD.S f17, f3, f5, f2
    const RV64Inst instr{FrvInst{FRV_FMADDS, 17, 3, 5, 2, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_fmadd_d) {
    // create instruction: FMADD.D f5, f4, f4, f4
    const RV64Inst instr{FrvInst{FRV_FMADDD, 5, 4, 4, 4, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_fmsub_f) {
    // create instruction: FMADD.S f5, f2, f4, f3
    const RV64Inst instr{FrvInst{FRV_FMSUBS, 5, 2, 4, 3, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_fmsub_d) {
    // create instruction: FMADD.D f1, f4, f6, f8
    const RV64Inst instr{FrvInst{FRV_FMSUBD, 1, 4, 6, 8, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_fnmadd_f) {
    // create instruction: FNMADD.S f4, f1, f2, f3
    const RV64Inst instr{FrvInst{FRV_FNMADDS, 4, 1, 2, 3, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_fnmadd_d) {
    // create instruction: FNMADD.D f8, f5, f6, f7
    const RV64Inst instr{FrvInst{FRV_FNMADDD, 8, 5, 6, 7, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_fnmsub_f) {
    // create instruction: FNMSUB.S f1, f1, f2, f3
    const RV64Inst instr{FrvInst{FRV_FNMSUBS, 1, 1, 2, 3, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}

TEST_F(TestFloatingPointLifting, test_fp_fnmsub_d) {
    // create instruction: FNMSUB.D f10, f2, f3, f16
    const RV64Inst instr{FrvInst{FRV_FNMSUBD, 10, 2, 3, 16, 0, 0}, 4};
    test_fp_arithmetic_lifting(instr);
}
