#include <lifter/lifter.h>

using namespace lifter::RV64;

std::variant<std::monostate, RoundingMode, SSAVar *> Lifter::parse_rounding_mode(BasicBlock *bb, reg_map &mapping, const uint64_t ip, const uint8_t riscv_rm) {
    switch (riscv_rm) {
    case 0:
    case 4:
        // both RISC-V Rounding Modes are rounding to the nearest, but ties will be rounded to even (0) respectively to max magnitude (4).
        // but this details cannot be saved in x86_64 and therefore both RISC-V rounding modes get mapped to the same IR rounding mode.
        return RoundingMode::NEAREST;
    case 1:
        return RoundingMode::ZERO;
    case 2:
        return RoundingMode::DOWN;
    case 3:
        return RoundingMode::UP;
    case 7:
        // dynamic rounding mode, read rounding mode from fcsr
        return get_csr(bb, mapping, ip, 2);
    default:
        assert(0 && "Unsupported or unkown rounding mode!");
        break;
    }
    return {};
}

void Lifter::lift_float_two_operands(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // check an invariant
    assert(is_float(op_size) && "This method is for lifting floating point instructions with two operands!");

    assert((instruction_type == Instruction::fmul || instruction_type == Instruction::fdiv || instruction_type == Instruction::add || instruction_type == Instruction::sub ||
            instruction_type == Instruction::min || instruction_type == Instruction::max) &&
           "This method handles only floating point instructions with two inputs!");

    SSAVar *const rs1 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);
    SSAVar *const rs2 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs2, ip, op_size);

    SSAVar *const dest = bb->add_var(op_size, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(instruction_type);
    op->lifter_info.in_op_size = op_size;
    op->set_inputs(rs1, rs2);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_sqrt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    // check an invariant
    assert(is_float(op_size) && "Sqrt only possible with floating points!");

    SSAVar *const src = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);

    // create the result variable
    SSAVar *const dest = bb->add_var(op_size, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(Instruction::fsqrt);
    op->lifter_info.in_op_size = op_size;
    op->set_inputs(src);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_fma(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // check some invariants
    assert(is_float(op_size) && "FMA only possible with floating points!");
    assert((instruction_type == Instruction::fmadd || instruction_type == Instruction::fmsub || instruction_type == Instruction::fnmadd || instruction_type == Instruction::fnmsub) &&
           "This function is for lifting fma instructions!");

    SSAVar *const rs1 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);
    SSAVar *const rs2 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs2, ip, op_size);
    SSAVar *const rs3 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs3, ip, op_size);

    // create the result variable
    SSAVar *const dest = bb->add_var(op_size, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(instruction_type);
    op->lifter_info.in_op_size = op_size;
    op->set_inputs(rs1, rs2, rs3);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_integer_conversion(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to, bool _signed) {
    const bool is_from_floating_point = is_float(from);
    const bool is_to_floating_point = is_float(to);

    // check some invariants
    assert(from != to && "A conversion from and to the same type is useless!");
    assert((is_from_floating_point || is_to_floating_point) && "For conversion, at least one variable has to be an floating point!");
    assert((!(is_from_floating_point && is_to_floating_point) || _signed) && "Conversion between floating points is always signed!");

    auto rounding_mode = parse_rounding_mode(bb, mapping, ip, instr.instr.misc);

    SSAVar *const src = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, from);

    // create the result variable
    SSAVar *dest = bb->add_var(to, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(_signed ? Instruction::convert : Instruction::uconvert);
    op->lifter_info.in_op_size = from;
    op->rounding_info = rounding_mode;
    op->set_inputs(src);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    if (cast_dir(to, Type::i64) == 1) {
        SSAVar *extended_dest = bb->add_var(Type::i64, ip);
        auto op = std::make_unique<Operation>(Instruction::sign_extend);
        op->lifter_info.in_op_size = Type::i32;
        op->set_inputs(dest);
        op->set_outputs(extended_dest);
        extended_dest->set_op(std::move(op));
        dest = extended_dest;
    }

    write_to_mapping(mapping, dest, instr.instr.rd, is_to_floating_point);
}

void Lifter::lift_float_sign_injection(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    assert(is_float(op_size) && "Sign injection is only possible with floating points!");

    const bool is_single_precision = op_size == Type::f32;

    // handle psudoinstructions
    if (instr.instr.rs1 == instr.instr.rs2) {
        switch (instr.instr.mnem) {
        case FRV_FSGNJNS:
        case FRV_FSGNJND: {
            // load mask to toggle sign bit
            SSAVar *mask = bb->add_var_imm(is_single_precision ? 0x8000'0000 : 0x8000'0000'0000'0000, ip);
            SSAVar *fp_mask = bb->add_var(op_size, ip);
            {
                auto op = std::make_unique<Operation>(Instruction::cast);
                op->lifter_info.in_op_size = is_single_precision ? Type::i32 : Type::i64;
                op->set_inputs(mask);
                op->set_outputs(fp_mask);
                fp_mask->set_op(std::move(op));
            }
            // negate value
            SSAVar *rs1 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);
            SSAVar *result = bb->add_var(op_size, ip);
            {
                auto op = std::make_unique<Operation>(Instruction::_xor);
                op->lifter_info.in_op_size = op_size;
                op->set_inputs(rs1, fp_mask);
                op->set_outputs(result);
                result->set_op(std::move(op));
            }
            write_to_mapping(mapping, result, instr.instr.rd, true);
            return;
        }
        case FRV_FSGNJXS:
        case FRV_FSGNJXD: {
            SSAVar *mask = bb->add_var_imm(is_single_precision ? 0x7FFF'FFFF : 0x7FFF'FFFF'FFFF'FFFF, ip);
            SSAVar *fp_mask = bb->add_var(op_size, ip);
            {
                auto op = std::make_unique<Operation>(Instruction::cast);
                op->lifter_info.in_op_size = is_single_precision ? Type::i32 : Type::i64;
                op->set_inputs(mask);
                op->set_outputs(fp_mask);
                fp_mask->set_op(std::move(op));
            }
            // caluclate absolute value
            SSAVar *rs1 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);
            SSAVar *result = bb->add_var(op_size, ip);
            {
                auto op = std::make_unique<Operation>(Instruction::_and);
                op->lifter_info.in_op_size = op_size;
                op->set_inputs(rs1, fp_mask);
                op->set_outputs(result);
                result->set_op(std::move(op));
            }
            write_to_mapping(mapping, result, instr.instr.rd, true);
            return;
        }
        default:
            break;
        }
    }

    // create mask to extract sign bit
    SSAVar *const sign_bit_extraction_mask = bb->add_var_imm(is_single_precision ? 0x80000000 : 0x8000000000000000, ip);
    SSAVar *const sign_zero_mask = bb->add_var_imm(is_single_precision ? 0x7FFFFFFF : 0x7FFFFFFFFFFFFFFF, ip);

    SSAVar *rs1 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);
    SSAVar *rs2 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs2, ip, op_size);

    const Type i_op_size = (op_size == Type::f32 ? Type::i32 : Type::i64);

    // cast fp inputs to integers to perform logical operations
    {
        SSAVar *const i_rs1 = bb->add_var(i_op_size, ip);
        auto op = std::make_unique<Operation>(Instruction::cast);
        op->lifter_info.in_op_size = op_size;
        op->set_inputs(rs1);
        op->set_outputs(i_rs1);
        i_rs1->set_op(std::move(op));
        rs1 = i_rs1;
    }
    {
        SSAVar *const i_rs2 = bb->add_var(i_op_size, ip);
        auto op = std::make_unique<Operation>(Instruction::cast);
        op->lifter_info.in_op_size = op_size;
        op->set_inputs(rs2);
        op->set_outputs(i_rs2);
        i_rs2->set_op(std::move(op));
        rs2 = i_rs2;
    }

    // mask the second source operand to extract the sign bit
    SSAVar *const sign_rs2 = bb->add_var(i_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = i_op_size;
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
        new_sign = bb->add_var(i_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_xor);
            op->lifter_info.in_op_size = i_op_size;
            op->set_inputs(sign_rs2, sign_bit_extraction_mask);
            op->set_outputs(new_sign);
            new_sign->set_op(std::move(op));
        }
        break;
    }
    case FRV_FSGNJXS:
    case FRV_FSGNJXD: {
        // mask the first source operand to extract the sign bit
        SSAVar *sign_rs1 = bb->add_var(i_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->lifter_info.in_op_size = i_op_size;
            op->set_inputs(rs1, sign_bit_extraction_mask);
            op->set_outputs(sign_rs1);
            sign_rs1->set_op(std::move(op));
        }

        // examine the new sign bit: sign_rs1 xor sign_rs2
        new_sign = bb->add_var(i_op_size, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_xor);
            op->lifter_info.in_op_size = i_op_size;
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
    SSAVar *const zero_signed_rs1 = bb->add_var(i_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = i_op_size;
        op->set_inputs(rs1, sign_zero_mask);
        op->set_outputs(zero_signed_rs1);
        zero_signed_rs1->set_op(std::move(op));
    }

    // change the sign of the first source operand to the sign of the second source operand: "sign or other_sign"
    SSAVar *const rs1_res = bb->add_var(i_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = i_op_size;
        op->set_inputs(zero_signed_rs1, new_sign);
        op->set_outputs(rs1_res);
        rs1_res->set_op(std::move(op));
    }

    // cast the calculated value back to fp
    SSAVar *const fp_res = bb->add_var(op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::cast);
        op->lifter_info.in_op_size = i_op_size;
        op->set_inputs(rs1_res);
        op->set_outputs(fp_res);
        fp_res->set_op(std::move(op));
    }

    write_to_mapping(mapping, fp_res, instr.instr.rd, true);
}

void Lifter::lift_float_move(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to) {
    const bool is_from_floating_point = is_float(from);
    const bool is_to_floating_point = is_float(to);

    // check an invariant
    assert((is_from_floating_point != is_to_floating_point) && "This method does only handle moves between floating point and integer registers!");

    // get the source variable from the mapping
    SSAVar *const src = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, from);

    // create the result variable
    SSAVar *dest = bb->add_var(to, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(Instruction::cast);
    op->lifter_info.in_op_size = from;
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
    assert(is_float(op_size) && "This method only handles floating points!");

    // get the source variables
    SSAVar *const rs1 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);
    SSAVar *const rs2 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs2, ip, op_size);

    // create the immediates used: 0 and 1
    SSAVar *const zero = bb->add_var_imm(0, ip);
    SSAVar *const one = bb->add_var_imm(1, ip);

    // create the result variable
    SSAVar *const dest = bb->add_var(Type::i64, ip);

    // create the operation and assign in- and outputs
    auto op = std::make_unique<Operation>(instruction_type);
    op->lifter_info.in_op_size = op_size;
    op->set_inputs(rs1, rs2, one, zero);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    // write the integer(!) variable to the mapping
    write_to_mapping(mapping, dest, instr.instr.rd, false);
}

void Lifter::lift_fclass(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    // check invariant
    assert(is_float(op_size) && "Only floating points are allowed here!");

    const bool is_single_precision = op_size == Type::f32;
    const Type integer_op_size = is_single_precision ? Type::i32 : Type::i64;

    // hint: all explanation with i. bit is zero indexed and counted from the lsb (least significant bit, lsb = 0. bit)
    // bit masks:

    SSAVar *const zero = bb->add_var_imm(0, ip);
    SSAVar *const mask0 = bb->add_var_imm(0x1, ip);   // lsb (0. bit) set if source is negative infinity.
    SSAVar *const mask3 = bb->add_var_imm(0x8, ip);   // 3. bit set if source is -0.
    SSAVar *const mask4 = bb->add_var_imm(0x10, ip);  // 4. bit set if source is +0.
    SSAVar *const mask7 = bb->add_var_imm(0x80, ip);  // 7. bit set if source is positive infinity.
    SSAVar *const mask8 = bb->add_var_imm(0x100, ip); // 8. bit set if source is signaling NaN.
    SSAVar *const mask9 = bb->add_var_imm(0x200, ip); // 9. bit set if source is quiet NaN.

    SSAVar *const combined_mask_1_2 = bb->add_var_imm(0b110, ip);     // combined mask with 1. and 2. bit set
    SSAVar *const combined_mask_5_6 = bb->add_var_imm(0b1100000, ip); // combined mask with 5. and 6. bit set
    SSAVar *const combined_mask_1_6 = bb->add_var_imm(0b1000010, ip); // combined mask with 1. and 6. bit set
    SSAVar *const combined_mask_2_5 = bb->add_var_imm(0b100100, ip);  // combined mask with 2. and 5. bit set
    SSAVar *const combined_mask_8_9 = bb->add_var_imm(0x300, ip);     // combined mask with 8. and 9. bit set
    SSAVar *const combined_mask_2_5_8 = bb->add_var_imm(0x124, ip);   // combined mask with 2., 5. and 8. bit set

    SSAVar *const exponent_mask = bb->add_var_imm(is_single_precision ? 0x7F800000 : 0x7FF0000000000000, ip); // mask to extract the exponent of the floating point number
    SSAVar *const mantisse_mask = bb->add_var_imm(is_single_precision ? 0x7FFFFF : 0xFFFFFFFFFFFFF, ip);      // mask to extract the mantisse of the floating point number

    SSAVar *const shift_to_right_amount = bb->add_var_imm(is_single_precision ? 31 : 63, ip);     // constant for shifting the sign bit to the lsb
    SSAVar *const mantisse_msb_shift_amount = bb->add_var_imm(is_single_precision ? 22 : 51, ip); // constant for shifting the msb of the mantisse to the lsb

    SSAVar *const negative_infinity = bb->add_var_imm(is_single_precision ? 0xff800000 : 0xfff0000000000000, ip); // bit pattern of -inf
    SSAVar *const positive_infinity = bb->add_var_imm(is_single_precision ? 0x7f800000 : 0x7FF0000000000000, ip); // bit pattern of +inf
    SSAVar *const negative_zero = bb->add_var_imm(is_single_precision ? 0x80000000 : 0x8000000000000000, ip);     // bit pattern of -0.0

    // cast the floating point bit pattern to integer bit pattern to use bit manipulation
    SSAVar *const f_src = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);

    SSAVar *const i_src = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::cast);
        op->lifter_info.in_op_size = op_size;
        op->set_inputs(f_src);
        op->set_outputs(i_src);
        i_src->set_op(std::move(op));
    }

    // shift the sign bit to the lsb
    SSAVar *const sign_bit = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::shr);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(i_src, shift_to_right_amount);
        op->set_outputs(sign_bit);
        sign_bit->set_op(std::move(op));
    }

    // set the value to the mask if the sign is negative (respectively non positive/zero, this saves the usage of an 'one' immediate variable)
    SSAVar *const negative_sign_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(sign_bit, zero, zero, combined_mask_1_2);
        op->set_outputs(negative_sign_test);
        negative_sign_test->set_op(std::move(op));
    }

    // set the value to the mask if the sign is positive
    SSAVar *const positive_sign_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(sign_bit, zero, combined_mask_5_6, zero);
        op->set_outputs(positive_sign_test);
        positive_sign_test->set_op(std::move(op));
    }

    // extract the exponent
    SSAVar *const exponent = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(i_src, exponent_mask);
        op->set_outputs(exponent);
        exponent->set_op(std::move(op));
    }

    // set the value to the mask if the exponent is zero
    SSAVar *const zero_exponent_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(exponent, zero, combined_mask_2_5, zero);
        op->set_outputs(zero_exponent_test);
        zero_exponent_test->set_op(std::move(op));
    }

    // set the value to the mask if the exponent is non zero
    SSAVar *const non_zero_exponent_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(exponent, zero, zero, combined_mask_1_6);
        op->set_outputs(non_zero_exponent_test);
        non_zero_exponent_test->set_op(std::move(op));
    }

    // set the value to the mask if all bits in the exponent are set
    SSAVar *const max_exponent_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(exponent, exponent_mask, combined_mask_8_9, zero);
        op->set_outputs(max_exponent_test);
        max_exponent_test->set_op(std::move(op));
    }

    SSAVar *const non_max_exponent = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(exponent, exponent_mask, zero, combined_mask_1_6);
        op->set_outputs(non_max_exponent);
        non_max_exponent->set_op(std::move(op));
    }

    // extract the mantisse
    SSAVar *const mantisse = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(i_src, mantisse_mask);
        op->set_outputs(mantisse);
        mantisse->set_op(std::move(op));
    }

    // shift the mantisse msb to the lsb
    SSAVar *const mantisse_msb = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::shr);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(mantisse, mantisse_msb_shift_amount);
        op->set_outputs(mantisse_msb);
        mantisse_msb->set_op(std::move(op));
    }

    // test whether the mantisse is not zero
    SSAVar *const mantisse_non_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(mantisse, zero, zero, combined_mask_2_5_8);
        op->set_outputs(mantisse_non_zero_test);
        mantisse_non_zero_test->set_op(std::move(op));
    }

    // test whether the msb of the mantisse is zero
    SSAVar *const mantisse_msb_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(mantisse_msb, zero, mask8, zero);
        op->set_outputs(mantisse_msb_zero_test);
        mantisse_msb_zero_test->set_op(std::move(op));
    }

    // test whether the msb of the mantisse is not zero
    SSAVar *const mantisse_msb_non_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(mantisse_msb, zero, zero, mask9);
        op->set_outputs(mantisse_msb_non_zero_test);
        mantisse_msb_non_zero_test->set_op(std::move(op));
    }

    // set to mask if it is a normal number (0 < exponent < max_exponent) (ignoring sign)
    SSAVar *const normal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(non_zero_exponent_test, non_max_exponent);
        op->set_outputs(normal_number_test);
        normal_number_test->set_op(std::move(op));
    }

    // set to mask if it is a subnormal number (exponent == 0 && mantisse != 0) (ignoring sign)
    SSAVar *const subnormal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(zero_exponent_test, mantisse_non_zero_test);
        op->set_outputs(subnormal_number_test);
        subnormal_number_test->set_op(std::move(op));
    }

    // test wheter the source is -inf
    SSAVar *const negative_inifinty_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(i_src, negative_infinity, mask0, zero);
        op->set_outputs(negative_inifinty_test);
        negative_inifinty_test->set_op(std::move(op));
    }

    // test wheter the source is a negative normal number (test for both conditions (normal number and negative sign bit)
    SSAVar *const negative_normal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(normal_number_test, negative_sign_test);
        op->set_outputs(negative_normal_number_test);
        negative_normal_number_test->set_op(std::move(op));
    }

    // test whether source is a negative subnormal number (test for both conditions are true (subnormal number and negative sign bit)
    SSAVar *const negative_subnormal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(subnormal_number_test, negative_sign_test);
        op->set_outputs(negative_subnormal_number_test);
        negative_subnormal_number_test->set_op(std::move(op));
    }

    // test whether source is -0.0
    SSAVar *const negative_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(i_src, negative_zero, mask3, zero);
        op->set_outputs(negative_zero_test);
        negative_zero_test->set_op(std::move(op));
    }

    // test whether source is +0.0
    SSAVar *const positive_zero_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(i_src, zero, mask4, zero);
        op->set_outputs(positive_zero_test);
        positive_zero_test->set_op(std::move(op));
    }

    // test whether source is a positive subnormal number (test for both conditions (subnormal number and positive sign bit)
    SSAVar *const positive_subnormal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(subnormal_number_test, positive_sign_test);
        op->set_outputs(positive_subnormal_number_test);
        positive_subnormal_number_test->set_op(std::move(op));
    }

    // test whether the source is a positive normal number (test for both conditions (normal number and positive sign bit)
    SSAVar *const positive_normal_number_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(normal_number_test, positive_sign_test);
        op->set_outputs(positive_normal_number_test);
        positive_normal_number_test->set_op(std::move(op));
    }

    // test whether the source is +inf
    SSAVar *const positive_inifinty_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::seq);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(i_src, positive_infinity, mask7, zero);
        op->set_outputs(positive_inifinty_test);
        positive_inifinty_test->set_op(std::move(op));
    }

    /* test whether source is a signaling NaN (all bits in the exponent are set && mantisse msb == 0 && mantisse != 0) */

    // test whether the mantisse has the "correct" form
    SSAVar *const signaling_nan_mantisse_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(mantisse_msb_zero_test, mantisse_non_zero_test);
        op->set_outputs(signaling_nan_mantisse_test);
        signaling_nan_mantisse_test->set_op(std::move(op));
    }

    // and the exponent is "correct"
    SSAVar *const signaling_nan_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(signaling_nan_mantisse_test, max_exponent_test);
        op->set_outputs(signaling_nan_test);
        signaling_nan_test->set_op(std::move(op));
    }

    // test whether source is a quiet NaN (all bits in the exponent are set && the msb of the mantisse is set)
    SSAVar *const quiet_nan_test = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(max_exponent_test, mantisse_msb_non_zero_test);
        op->set_outputs(quiet_nan_test);
        quiet_nan_test->set_op(std::move(op));
    }

    // merge tests which sets the bits 0 and 1
    SSAVar *const test_0_1 = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(negative_inifinty_test, negative_normal_number_test);
        op->set_outputs(test_0_1);
        test_0_1->set_op(std::move(op));
    }

    // merge tests which sets the bits 2 and 3
    SSAVar *const test_2_3 = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(negative_subnormal_number_test, negative_zero_test);
        op->set_outputs(test_2_3);
        test_2_3->set_op(std::move(op));
    }

    // merge tests which sets the bits 4 and 5
    SSAVar *const test_4_5 = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(positive_zero_test, positive_subnormal_number_test);
        op->set_outputs(test_4_5);
        test_4_5->set_op(std::move(op));
    }

    // merge tests which sets the bits 6 and 7
    SSAVar *const test_6_7 = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(positive_normal_number_test, positive_inifinty_test);
        op->set_outputs(test_6_7);
        test_6_7->set_op(std::move(op));
    }

    // merge tests which sets the bits 8 and 9
    SSAVar *const test_8_9 = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(signaling_nan_test, quiet_nan_test);
        op->set_outputs(test_8_9);
        test_8_9->set_op(std::move(op));
    }

    // merge tests which sets the bits from 0 to 3
    SSAVar *const test_0_to_3 = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(test_0_1, test_2_3);
        op->set_outputs(test_0_to_3);
        test_0_to_3->set_op(std::move(op));
    }

    // merge tests which sets the bits from 4 to 7
    SSAVar *const test_4_to_7 = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(test_4_5, test_6_7);
        op->set_outputs(test_4_to_7);
        test_4_to_7->set_op(std::move(op));
    }

    // merge tests which sets the bits from 0 to 7
    SSAVar *const test_0_to_7 = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(test_0_to_3, test_4_to_7);
        op->set_outputs(test_0_to_7);
        test_0_to_7->set_op(std::move(op));
    }

    // merge to the test result
    SSAVar *const test_result = bb->add_var(integer_op_size, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_or);
        op->lifter_info.in_op_size = integer_op_size;
        op->set_inputs(test_0_to_7, test_8_9);
        op->set_outputs(test_result);
        test_result->set_op(std::move(op));
    }

    // write the test result to the mapping
    write_to_mapping(mapping, test_result, instr.instr.rd);
}
