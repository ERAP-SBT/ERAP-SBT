#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_shift(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction &instruction_type, const Type &op_size) {
    // prepare for shift, only use lower 5bits

    SSAVar *mask;

    // cast immediate from 64bit to 32bit if instruction has 32bit size
    if (op_size == Type::i32) {
        mask = load_immediate(bb, (int32_t)0x1F, ip, false);
    } else {
        mask = load_immediate(bb, (int64_t)0x1F, ip, false);
    }

    // create new variable with the result of the masking
    SSAVar *masked_count_shifts = bb->add_var(op_size, ip, instr.instr.rs2);
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::_and);
    operation->set_inputs(mapping.at(instr.instr.rs2), mask);
    operation->set_outputs(masked_count_shifts);
    mapping.at(instr.instr.rs2) = masked_count_shifts;

    lift_arithmetical_logical(bb, instr, mapping, ip, instruction_type, op_size);
}

void Lifter::lift_shift_immediate(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction &instruction_type, const Type &op_size) {
    // masking the operand
    instr.instr.imm = instr.instr.imm & 0x1F;
    lift_arithmetical_logical_immediate(bb, instr, mapping, ip, instruction_type, op_size);
}
