#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_sqrt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    assert((op_size == Type::f32 || op_size == Type::f64) && "Sqrt only possible with floating points!");

    SSAVar *src = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);

    assert(src->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    SSAVar *dest = bb->add_var(op_size, ip, instr.instr.rd + START_IDX_FLOATING_POINT_STATICS);
    auto op = std::make_unique<Operation>(Instruction::fsqrt);
    op->set_inputs(src);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_min_max(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    assert((op_size == Type::f32 || op_size == Type::f64) && "Min/Max only possible with floating points!");
    assert((instruction_type == Instruction::fmin || instruction_type == Instruction::fmax) && "Only fmin and fmax are allowed here!");

    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, true);

    assert(rs1->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs2->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    SSAVar *dest = bb->add_var(op_size, ip, instr.instr.rd + START_IDX_FLOATING_POINT_STATICS);
    auto op = std::make_unique<Operation>(instruction_type);
    op->set_inputs(rs1, rs2);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_fma(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    assert((op_size == Type::f32 || op_size == Type::f64) && "FMA only possible with floating points!");
    assert((instruction_type == Instruction::ffmadd || instruction_type == Instruction::ffmsub || instruction_type == Instruction::ffnmadd || instruction_type == Instruction::ffnmsub) &&
           "This function is for lifting fma instructions!");

    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, true);
    SSAVar *rs3 = get_from_mapping(bb, mapping, instr.instr.rs3, ip, true);

    assert(rs1->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs2->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs3->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    SSAVar *dest = bb->add_var(op_size, ip, instr.instr.rd + START_IDX_FLOATING_POINT_STATICS);
    auto op = std::make_unique<Operation>(instruction_type);
    op->set_inputs(rs1, rs2, rs3);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_integer_conversion(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to, bool _signed) {
    assert(from != to && "A conversion from and to the same type is useless!");

    bool is_from_floating_point = from == Type::f32 || from == Type::f64;
    bool is_to_floating_point = to == Type::f32 || to == Type::f64;

    assert ((is_from_floating_point || is_to_floating_point) && "For conversion, at least one variable has to be an floating point!");
    assert (is_from_floating_point && is_to_floating_point && _signed && "Conversion between floating points is always signed!");
    
    SSAVar *src = get_from_mapping(bb, mapping, instr.instr.rs1, ip, is_from_floating_point);

    SSAVar *dest = bb->add_var(to, ip, instr.instr.rd + START_IDX_FLOATING_POINT_STATICS);
    auto op = std::make_unique<Operation>(_signed ? Instruction::convert : Instruction::uconvert);
    op->set_inputs(src);
    op->set_outputs(dest);
    dest->set_op(std::move(op));

    write_to_mapping(mapping, dest, instr.instr.rd, is_to_floating_point);
}