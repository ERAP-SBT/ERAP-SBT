#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_sqrt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    // check an invariant
    assert((op_size == Type::f32 || op_size == Type::f64) && "Sqrt only possible with floating points!");

    SSAVar *src = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);

    // check another invariant
    assert(src->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    // create the result variable
    SSAVar *dest = bb->add_var(op_size, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(Instruction::fsqrt);
    op->set_inputs(src);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_min_max(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // check some invariants
    assert((op_size == Type::f32 || op_size == Type::f64) && "Min/Max only possible with floating points!");
    assert((instruction_type == Instruction::fmin || instruction_type == Instruction::fmax) && "Only fmin and fmax are allowed here!");

    // get the source variables
    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, true);

    // more invariants
    assert(rs1->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs2->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    // create the result variable
    SSAVar *dest = bb->add_var(op_size, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(instruction_type);
    op->set_inputs(rs1, rs2);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_fma(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // check some invariants
    assert((op_size == Type::f32 || op_size == Type::f64) && "FMA only possible with floating points!");
    assert((instruction_type == Instruction::ffmadd || instruction_type == Instruction::ffmsub || instruction_type == Instruction::ffnmadd || instruction_type == Instruction::ffnmsub) &&
           "This function is for lifting fma instructions!");

    // get the source variables from the mapping
    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, true);
    SSAVar *rs3 = get_from_mapping(bb, mapping, instr.instr.rs3, ip, true);

    // more invariants
    assert(rs1->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs2->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs3->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    // create the result variable
    SSAVar *dest = bb->add_var(op_size, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(instruction_type);
    op->set_inputs(rs1, rs2, rs3);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_integer_conversion(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to, bool _signed) {
    bool is_from_floating_point = from == Type::f32 || from == Type::f64;
    bool is_to_floating_point = to == Type::f32 || to == Type::f64;

    // check some invariants
    assert(from != to && "A conversion from and to the same type is useless!");
    assert((is_from_floating_point || is_to_floating_point) && "For conversion, at least one variable has to be an floating point!");
    assert(((is_from_floating_point && is_to_floating_point) ? _signed : true) && "Conversion between floating points is always signed!");

    // get the source variable from the mapping
    SSAVar *src = get_from_mapping(bb, mapping, instr.instr.rs1, ip, is_from_floating_point);

    // create the result variable
    SSAVar *dest = bb->add_var(to, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(_signed ? Instruction::convert : Instruction::uconvert);
    op->set_inputs(src);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, is_to_floating_point);
}

void Lifter::lift_float_sign_injection(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    assert((op_size == Type::f32 || op_size == Type::f64) && "Sign injection is only possible with floating points!");

    // create mask to extract sign bit
    SSAVar *sign_bit_extraction_mask = bb->add_var_imm(op_size == Type::f32 ? 0x80000000 : 0x8000000000000000, ip);
    SSAVar *sign_zero_mask = bb->add_var_imm(op_size == Type::f32 ? 0x7FFFFFFF : 0x7FFFFFFFFFFFFFFF, ip);

    // get the source operands
    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, true);

    // mask the second source operand to extract the sign bit
    SSAVar *sign_rs2 = bb->add_var(op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(rs2, sign_bit_extraction_mask);
        op->set_outputs(sign_rs2);
        sign_rs2->set_op(std::move(op));
    }

    // create mask
    SSAVar *new_sign;
    switch (instr.instr.mnem) {
    case FRV_FSGNJS:
    case FRV_FSGNJD: {
        // the new sign of rs1 is the sign of rs2
        new_sign = sign_rs2;
        break;
    }
    case FRV_FSGNJNS:
    case FRV_FSGNJND: {
        // the new sign of rs1 is the negation of the sign of rs2: "sign xor 1"
        new_sign = bb->add_var(op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_xor);
            op->set_inputs(sign_rs2, sign_bit_extraction_mask);
            op->set_outputs(new_sign);
            new_sign->set_op(std::move(op));
        }
        break;
    }
    case FRV_FSGNJXS:
    case FRV_FSGNJXD: {
        // mask the first source operand to extract the sign bit
        SSAVar *sign_rs1 = bb->add_var(op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->set_inputs(rs1, sign_bit_extraction_mask);
            op->set_outputs(sign_rs1);
            sign_rs2->set_op(std::move(op));
        }

        // examine the new sign bit: sign_rs1 xor sign_rs2
        new_sign = bb->add_var(op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_xor);
            op->set_inputs(sign_rs1, sign_rs2);
            op->set_outputs(new_sign);
            new_sign->set_op(std::move(op));
        }
        break;
    }
    default: {
        assert(0 && "The instruction is no sign injection!");
        new_sign = nullptr;
        break;
    }
    }

    // set the sign of the first operand to zero: "sign and 0"
    SSAVar *zero_signed_rs1 = bb->add_var(op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(rs1, sign_zero_mask);
        op->set_outputs(zero_signed_rs1);
        zero_signed_rs1->set_op(std::move(op));
    }

    // change the sign of the first source operand to the sign of the second source operand: "sign or other_sign"
    SSAVar *rs1_res = bb->add_var(op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->set_inputs(zero_signed_rs1, new_sign);
        op->set_outputs(rs1_res);
        rs1_res->set_op(std::move(op));
    }

    write_to_mapping(mapping, rs1_res, instr.instr.rd, true);
}

void Lifter::lift_float_move(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to) {
    bool is_from_floating_point = from == Type::f32 || from == Type::f64;
    bool is_to_floating_point = to == Type::f32 || to == Type::f64;

    // check an invariant
    assert((is_from_floating_point != is_to_floating_point) && "This method does only handle moves between floating point and integer registers!");

    // get the source variable from the mapping
    SSAVar *src = get_from_mapping(bb, mapping, instr.instr.rs1, ip, is_from_floating_point);

    // create the result variable
    SSAVar *dest = bb->add_var(to, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(Instruction::cast);
    op->set_inputs(src);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    // sign extent if necessary
    if (to == Type::i32) {
        std::optional<SSAVar *> result_optional = convert_type(bb, ip, dest, Type::i64);
        assert(result_optional.has_value());
        dest = result_optional.value();
    }

    write_to_mapping(mapping, dest, instr.instr.rd, is_to_floating_point);
}

void Lifter::lift_float_comparison(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // check some invariants
    assert((instruction_type == Instruction::sle || instruction_type == Instruction::slt || instruction_type == Instruction::seq) &&
           "This methods only handles the floating point comparisons 'sle', 'slt' and 'seq'!");
    assert((op_size == Type::f32 || op_size == Type::f64) && "This method only handles floating points!");

    // get the source variables
    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, true);

    // create the immediates used: 0 and 1
    SSAVar *zero = bb->add_var_imm(0, ip);
    SSAVar *one = bb->add_var_imm(1, ip);

    // create the result variable
    SSAVar *dest = bb->add_var(Type::i64, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(instruction_type);
    op->set_inputs(rs1, rs2, one, zero);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    // write the integer(!) variable to the mapping
    write_to_mapping(mapping, dest, instr.instr.rd, false);
}

void Lifter::lift_fclass(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    // check invariant
    assert((op_size == Type::f32 || op_size == Type::f64) && "Only floating points are allowed here!");

    bool is_single_precision = op_size == Type::f32;
    const Type integer_op_size = is_single_precision ? Type::i32 : Type::i64;

    // hint: all explanation with i. bit is zero indexed and counted from the lsb (least significant bit, lsb = 0. bit)
    // bit masks:

    SSAVar *zero = bb->add_var_imm(0, ip);
    SSAVar *mask1 = bb->add_var_imm(0x1, ip);    // lsb (0. bit) set if source is negative infinity.
    SSAVar *mask4 = bb->add_var_imm(0x8, ip);    // 3. bit set if source is -0.
    SSAVar *mask5 = bb->add_var_imm(0x10, ip);   // 4. bit set if source is +0.
    SSAVar *mask8 = bb->add_var_imm(0x80, ip);   // 7. bit set if source is positive infinity.
    SSAVar *mask9 = bb->add_var_imm(0x100, ip);  // 8. bit set if source is signaling NaN.
    SSAVar *mask10 = bb->add_var_imm(0x200, ip); // 9. bit set if source is quiet NaN.

    SSAVar *combined_mask_1_2 = bb->add_var_imm(0b110, ip);     // combined mask with 1. and 2. bit set
    SSAVar *combined_mask_5_6 = bb->add_var_imm(0b1100000, ip); // combined mask with 5. and 6. bit set
    SSAVar *combined_mask_1_6 = bb->add_var_imm(0b1000010, ip); // combined mask with 1. and 6. bit set
    SSAVar *combined_mask_2_5 = bb->add_var_imm(0b100100, ip);  // combined mask with 2. and 5. bit set
    SSAVar *combined_mask_8_9 = bb->add_var_imm(0x200, ip);     // combined mask with 8. and 9. bit set

    SSAVar *exponent_mask = bb->add_var_imm(is_single_precision ? 0x7F800000 : 0x7FF0000000000000, ip); // mask to extract the exponent of the floating point number
    SSAVar *mantisse_mask = bb->add_var_imm(is_single_precision ? 0x7FFFFF : 0xFFFFFFFFFFFFF, ip);      // mask to extract the mantisse of the floating point number

    SSAVar *shift_to_right_amount = bb->add_var_imm(is_single_precision ? 31 : 63, ip);     // constant for shifting the sign bit to the lsb
    SSAVar *mantisse_msb_shift_amount = bb->add_var_imm(is_single_precision ? 22 : 51, ip); // constant for shifting the msb of the mantisse to the slb

    SSAVar *negative_infinity = bb->add_var_imm(is_single_precision ? 0xff800000 : 0xfff0000000000000, ip); // bit pattern of -inf
    SSAVar *positive_infinity = bb->add_var_imm(is_single_precision ? 0x7f800000 : 0x7FF0000000000000, ip); // bit pattern of +inf
    SSAVar *negative_zero = bb->add_var_imm(is_single_precision ? 0x80000000 : 0x8000000000000000, ip);     // bit pattern of -0.0

    SSAVar *max_exponent_value = bb->add_var_imm(is_single_precision ? 0xFF : 0x7FF, ip); // bit pattern of exponent with all bits set

    // cast the floating point bit pattern to integer bit pattern to use bit manipulation
    SSAVar *f_src = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);
    SSAVar *i_src = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::cast);
        op->set_inputs(f_src);
        op->set_outputs(i_src);
        i_src->set_op(std::move(op));
    }

    // shift the sign bit to the lsb
    SSAVar *sign_bit = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::shr);
        op->set_inputs(i_src, shift_to_right_amount);
        op->set_outputs(sign_bit);
        sign_bit->set_op(std::move(op));
    }

    // set the value to the mask if the sign is negative (respectively non positive/zero, this saves the usage of an 'one' immediate variable)
    SSAVar *negative_sign_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(i_src, zero, zero, combined_mask_1_2);
        op->set_outputs(negative_sign_test);
        negative_sign_test->set_op(std::move(op));
    }

    // set the value to the mask if the sign is positive
    SSAVar *positive_sign_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(i_src, zero, combined_mask_5_6, zero);
        op->set_outputs(negative_sign_test);
        negative_sign_test->set_op(std::move(op));
    }

    // extract the exponent
    SSAVar *exponent = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(i_src, exponent_mask);
        op->set_outputs(exponent);
        exponent->set_op(std::move(op));
    }

    // set the value to the mask if the exponent is zero
    SSAVar *zero_exponent_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(exponent, zero, combined_mask_2_5, zero);
        op->set_outputs(zero_exponent_test);
        zero_exponent_test->set_op(std::move(op));
    }

    // set the value to the mask if the exponent is non zero
    SSAVar *non_zero_exponent_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(exponent, zero, zero, combined_mask_1_6);
        op->set_outputs(non_zero_exponent_test);
        non_zero_exponent_test->set_op(std::move(op));
    }

    // set the value to the mask if all bits in the exponent are set
    SSAVar *max_exponent_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(exponent, max_exponent_value, combined_mask_8_9, zero);
        op->set_outputs(max_exponent_test);
        max_exponent_test->set_op(std::move(op));
    }

    // extract the mantisse
    SSAVar *mantisse = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(i_src, mantisse_mask);
        op->set_outputs(mantisse);
        mantisse->set_op(std::move(op));
    }

    // shift the mantisse msb to the lsb
    SSAVar *mantisse_msb = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::shr);
        op->set_inputs(mantisse, mantisse_msb_shift_amount);
        op->set_outputs(mantisse_msb);
        mantisse_msb->set_op(std::move(op));
    }

    // test whether the mantisse is not zero
    SSAVar *mantisse_non_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(mantisse, zero, zero, mask9);
        op->set_outputs(mantisse_non_zero_test);
        mantisse_non_zero_test->set_op(std::move(op));
    }

    // test whether the msb of the mantisse is zero
    SSAVar *mantisse_msb_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(mantisse_msb, zero, mask9, zero);
        op->set_outputs(mantisse_msb_zero_test);
        mantisse_msb_zero_test->set_op(std::move(op));
    }

    // test whether the msb of the mantisse is not zero
    SSAVar *mantisse_msb_non_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(mantisse_msb, zero, zero, mask10);
        op->set_outputs(mantisse_msb_non_zero_test);
        mantisse_msb_non_zero_test->set_op(std::move(op));
    }

    // test wheter the source is -inf
    SSAVar *negative_inifinty_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(i_src, negative_infinity, mask1, zero);
        op->set_outputs(negative_inifinty_test);
        negative_inifinty_test->set_op(std::move(op));
    }

    // test wheter the source is a negative normal number (test for both conditions (normal number and negative sign bit)
    SSAVar *negative_normal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(non_zero_exponent_test, negative_sign_test);
        op->set_outputs(negative_normal_number_test);
        negative_normal_number_test->set_op(std::move(op));
    }

    // test whether source is a negative subnormal number (test for both conditions are true (subnormal number and negative sign bit)
    SSAVar *negative_subnormal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(zero_exponent_test, negative_sign_test);
        op->set_outputs(negative_subnormal_number_test);
        negative_subnormal_number_test->set_op(std::move(op));
    }

    // test whether source is -0.0
    SSAVar *negative_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(i_src, negative_zero, mask4, zero);
        op->set_outputs(negative_zero_test);
        negative_zero_test->set_op(std::move(op));
    }

    // test whether source is +0.0
    SSAVar *positive_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(i_src, zero, mask5, zero);
        op->set_outputs(positive_zero_test);
        positive_zero_test->set_op(std::move(op));
    }

    // test whether source is a positive subnormal number (test for both conditions (subnormal number and positive sign bit)
    SSAVar *positive_subnormal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(zero_exponent_test, negative_sign_test);
        op->set_outputs(positive_subnormal_number_test);
        positive_subnormal_number_test->set_op(std::move(op));
    }

    // test whether the source is a positive normal number (test for both conditions (normal number and positive sign bit)
    SSAVar *positive_normal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(non_zero_exponent_test, positive_sign_test);
        op->set_outputs(positive_normal_number_test);
        positive_normal_number_test->set_op(std::move(op));
    }

    // test whether the source is +inf
    SSAVar *positive_inifinty_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->set_inputs(i_src, positive_infinity, mask8, zero);
        op->set_outputs(positive_inifinty_test);
        positive_inifinty_test->set_op(std::move(op));
    }

    // test whether source is a signaling NaN (all bits in the exponent are set && mantisse msb == 0 && mantisse != 0)
    SSAVar *signaling_nan_test = bb->add_var(integer_op_size, ip);
    {
        // test whether the mantisse has the "correct" form
        SSAVar *signaling_nan_mantisse_test = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->set_inputs(mantisse_msb_zero_test, mantisse_non_zero_test);
            op->set_outputs(signaling_nan_test);
            signaling_nan_test->set_op(std::move(op));
        }

        // and the exponent is "correct"
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->set_inputs(signaling_nan_mantisse_test, max_exponent_test);
            op->set_outputs(signaling_nan_test);
            signaling_nan_test->set_op(std::move(op));
        }
    }

    // test whether source is a quiet NaN (all bits in the exponent are set && the msb of the mantisse is set)
    SSAVar *quiet_nan_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(max_exponent_test, mantisse_msb_non_zero_test);
        op->set_outputs(quiet_nan_test);
        quiet_nan_test->set_op(std::move(op));
    }

    // merge all test results with logical or's
    SSAVar *test_result = bb->add_var(integer_op_size, ip);
    {
        // merge tests which sets the bits 0 and 1
        SSAVar *test_0_1 = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(negative_inifinty_test, negative_normal_number_test);
            op->set_outputs(test_0_1);
            test_0_1->set_op(std::move(op));
        }

        // merge tests which sets the bits 2 and 3
        SSAVar *test_2_3 = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(negative_subnormal_number_test, negative_zero_test);
            op->set_outputs(test_2_3);
            test_2_3->set_op(std::move(op));
        }

        // merge tests which sets the bits 4 and 5
        SSAVar *test_4_5 = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(positive_zero_test, positive_subnormal_number_test);
            op->set_outputs(test_4_5);
            test_4_5->set_op(std::move(op));
        }

        // merge tests which sets the bits 6 and 7
        SSAVar *test_6_7 = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(positive_normal_number_test, positive_inifinty_test);
            op->set_outputs(test_6_7);
            test_6_7->set_op(std::move(op));
        }

        // merge tests which sets the bits 8 and 9
        SSAVar *test_8_9 = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(signaling_nan_test, quiet_nan_test);
            op->set_outputs(test_8_9);
            test_8_9->set_op(std::move(op));
        }

        // merge tests which sets the bits from 0 to 3
        SSAVar *test_0_to_3 = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(test_0_1, test_2_3);
            op->set_outputs(test_0_to_3);
            test_0_to_3->set_op(std::move(op));
        }

        // merge tests which sets the bits from 4 to 7
        SSAVar *test_4_to_7 = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(test_4_5, test_6_7);
            op->set_outputs(test_4_to_7);
            test_4_to_7->set_op(std::move(op));
        }

        // merge tests which sets the bits from 0 to 7
        SSAVar *test_0_to_7 = bb->add_var(integer_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(test_0_to_3, test_4_to_7);
            op->set_outputs(test_0_to_7);
            test_0_to_7->set_op(std::move(op));
        }

        // merge to the test result
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(test_0_to_7, test_8_9);
            op->set_outputs(test_result);
            test_result->set_op(std::move(op));
        }
    }

    // write the test result to the mapping
    write_to_mapping(mapping, test_result, instr.instr.rd, false);
}