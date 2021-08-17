#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_shift(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // prepare for shift, only use lower 5bits

    SSAVar *mask;

    // cast immediate from 64bit to 32bit if instruction has 32bit size
    if (op_size == Type::i32) {
        mask = load_immediate(bb, (int32_t)0x3F, ip, false);
    } else {
        mask = load_immediate(bb, (int64_t)0x3F, ip, false);
    }

    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip);

    // create new variable with the result of the masking
    SSAVar *masked_count_shifts = bb->add_var(op_size, ip, instr.instr.rs2);
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::_and);
    operation->set_inputs(rs2, mask);
    operation->set_outputs(masked_count_shifts);

    write_to_mapping(mapping, masked_count_shifts, instr.instr.rs2);

    lift_arithmetical_logical(bb, instr, mapping, ip, instruction_type, op_size);
}

void Lifter::lift_shift_immediate(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // masking the operand (by modifying the immediate)
    const RV64Inst instr_cp = RV64Inst{FrvInst{instr.instr.mnem, instr.instr.rd, instr.instr.rs1, instr.instr.rs2, instr.instr.rs3, instr.instr.misc, instr.instr.imm & 0x3F}, instr.size};
    lift_arithmetical_logical_immediate(bb, instr_cp, mapping, ip, instruction_type, op_size);
}
