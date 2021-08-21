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
    assert((instruction_type == Instruction::fle || instruction_type == Instruction::flt || instruction_type == Instruction::feq) &&
           "This methods only handles the floating point comparisons 'fle', 'flt' and 'feq'!");
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