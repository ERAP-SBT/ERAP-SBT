#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_sqrt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    assert((op_size == Type::f32 || op_size == Type::f64) && "Sqrt only possible with floating points!");

    SSAVar *src = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);

    assert(src->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    SSAVar *dest = bb->add_var(op_size, ip);
    auto op = std::make_unique<Operation>(Instruction::fsqrt);
    op->set_inputs(src);
    op->set_outputs(dest);
    dest->set_op(op);

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_min_max(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    assert((op_size == Type::f32 || op_size == Type::f64) && "Min/Max only possible with floating points!");
    assert((instruction_type == Instruction::fmin || instruction_type == Instruction::fmax) && "Only fmin and fmax are allowed here!");

    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip, true);
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, true);

    assert(rs1->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs2->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    SSAVar *dest = bb->add_var(op_size, ip);
    auto op = std::make_unique<Operation>(instruction_type);
    op->set_inputs(rs1, rs2);
    op->set_outputs(dest);
    dest->set_op(op);

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}

void Lifter::lift_float_fma(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    assert((op_size == Type::f32 || op_size == Type::f64) && "FMA only possible with floating points!");
    assert((instruction_type == Instruction::ffmadd || instruction_type == Instruction::ffmsub || instruction_type == Instruction::ffnmadd || instruction_type == Instruction::ffnmsub) &&
           "This function is for lifting fma instructions!");

    SSAVar *rs1 = get_from_mapping(bb, mppaing, instr.instr.rs1, ip, true);
    SSAVar *rs2 = get_from_mapping(bb, mppaing, instr.instr.rs2, ip, true);
    SSAVar *rs3 = get_from_mapping(bb, mppaing, instr.instr.rs3, ip, true);

    assert(rs1->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs2->type == op_size && "Calculation with different sizes of floating points aren't possible!");
    assert(rs3->type == op_size && "Calculation with different sizes of floating points aren't possible!");

    SSAVar *dest = bb->add_var(op_size, ip);
    auto op = std::make_unique<Operation>(instruction_type);
    op->set_inputs(rs1, rs2, rs3);
    op->set_outputs(dest);
    dest->set_op(op);

    write_to_mapping(mapping, dest, instr.instr.rd, true);
}