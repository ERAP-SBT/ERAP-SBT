#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_ecall(BasicBlock *bb, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    // the behavior of the ECALL instruction is system dependant. (= SYSCALL)
    // we give the syscall the address at which the program control flow continues (= next basic block)
    CfOp &ecall_op = bb->add_cf_op(CFCInstruction::syscall, nullptr, ip, next_addr);

    // the syscall number is required from register a7 (=x17) + args in a0 - a5
    ecall_op.set_inputs(mapping[17], // a7
                        mapping[10], // a0
                        mapping[11], // a1
                        mapping[12], // a2
                        mapping[13], // a3
                        mapping[14], // a4
                        mapping[15]  // a5
    );

    // the result should be placed in the statics for register a0 (x10) and a1 (x11)
    std::get<CfOp::SyscallInfo>(ecall_op.info).static_mapping = {10, 11};
}

void Lifter::lift_branch(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    // ((rs1 == rs2) ? continue : jmp to addr) := true
    // ((rs1 == rs2) ? jmp to addr : continue) := false
    bool reverse_jumps = false;

    // parse the branching instruction -> jumps if the condition is true
    CfOp &c_jmp = bb->add_cf_op(CFCInstruction::cjump, nullptr, ip);

    switch (instr.instr.mnem) {
    case FRV_BEQ:
        // BEQ: (rs1 == rs2) ? jmp to addr : continue
        std::get<CfOp::CJumpInfo>(c_jmp.info).type = CfOp::CJumpInfo::CJumpType::eq;
        break;
    case FRV_BNE:
        // BNE: (rs1 == rs2) ? continue : jmp to addr
        std::get<CfOp::CJumpInfo>(c_jmp.info).type = CfOp::CJumpInfo::CJumpType::eq;
        reverse_jumps = true;
        break;
    case FRV_BLT:
        // BLT: (rs1 < rs2) ? jmp to addr : continue
        std::get<CfOp::CJumpInfo>(c_jmp.info).type = CfOp::CJumpInfo::CJumpType::slt;
        break;
    case FRV_BGE:
        // BGE: (rs1 < rs2) ? continue : jmp to addr
        std::get<CfOp::CJumpInfo>(c_jmp.info).type = CfOp::CJumpInfo::CJumpType::slt;
        reverse_jumps = true;
        break;
    case FRV_BLTU:
        // BLTU: (rs1 <u rs2) ? jmp to addr : continue
        std::get<CfOp::CJumpInfo>(c_jmp.info).type = CfOp::CJumpInfo::CJumpType::lt;
        break;
    case FRV_BGEU:
        // BGEU: (rs1 <u rs2) ? continue : jmp to addr
        std::get<CfOp::CJumpInfo>(c_jmp.info).type = CfOp::CJumpInfo::CJumpType::lt;
        reverse_jumps = true;
        break;
    }

    // calculate jump address and create an immediate variable for the jump and the continuation address
    const uint64_t jump_addr = static_cast<uint64_t>(static_cast<int64_t>(instr.instr.imm)) + ip;
    SSAVar *jump_addr_variable = bb->add_var_imm(jump_addr, ip, true);
    SSAVar *next_addr_var = bb->add_var_imm(next_addr, ip, true);

    // stores the address which is used if the branch condition is false
    const uint64_t uc_jmp_addr = reverse_jumps ? jump_addr : next_addr;
    SSAVar *uc_jmp_addr_var = reverse_jumps ? jump_addr_variable : next_addr_var;

    // stores the address which is used if the branch condition is true
    const uint64_t br_jmp_addr = reverse_jumps ? next_addr : jump_addr;
    SSAVar *br_jmp_addr_var = reverse_jumps ? next_addr_var : jump_addr_variable;

    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip);

    c_jmp.set_inputs(rs1, rs2, br_jmp_addr_var);
    std::get<CfOp::LifterInfo>(c_jmp.lifter_info).jump_addr = br_jmp_addr;

    // Branch not taken -> like JAL, but doesn't write return address to register
    CfOp &continue_jmp = bb->add_cf_op(CFCInstruction::jump, nullptr, ip, uc_jmp_addr);
    continue_jmp.set_inputs(uc_jmp_addr_var);
}

void Lifter::lift_jal(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    // 1. calculate the jump address
    const uint64_t jump_addr = static_cast<uint64_t>(static_cast<int64_t>(instr.instr.imm)) + ip;

    // 2. load the jump address as variable
    SSAVar *jump_addr_variable = bb->add_var_imm(jump_addr, ip, true);

    if (instr.instr.rd != ZERO_IDX) {
        // 3. load return address as another immediate
        SSAVar *return_addr = bb->add_var_imm(next_addr, ip, true);

        // write SSAVar of the result of the operation back to mapping
        write_to_mapping(mapping, return_addr, instr.instr.rd);
    }

    // 4. jump!
    // According to the risc-v manual, direct jumps which write the ip to x1 (or ra) are considered subroutine calls.
    // create the jump operation
    CfOp &cf_operation = bb->add_cf_op((optimizations & OPT_CALL_RET) && is_link_reg(instr.instr.rd) ? CFCInstruction::call : CFCInstruction::jump, nullptr, ip, jump_addr);

    // set operation in- and outputs
    cf_operation.set_inputs(jump_addr_variable);
}

void Lifter::lift_jalr(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    // detect indirect jumps that are just returns
    if ((optimizations & OPT_CALL_RET) && instr.instr.imm == 0 && is_link_reg(instr.instr.rs1) && instr.instr.rd == 0) {
        CfOp &return_op = bb->add_cf_op(CFCInstruction::_return, nullptr, ip);
        return_op.set_inputs(get_from_mapping(bb, mapping, instr.instr.rs1, ip));
        return;
    }

    // the address is encoded as an immediate offset....
    // 1. load the immediate offset
    SSAVar *immediate = load_immediate(bb, (int64_t)instr.instr.imm, ip, false);

    // 2. add the offset register (the jalR-specific part)
    SSAVar *offset_register = get_from_mapping(bb, mapping, instr.instr.rs1, ip);

    SSAVar *sum = bb->add_var(Type::i64, ip);
    {
        auto addition = std::make_unique<Operation>(Instruction::add);
        addition->lifter_info.in_op_size = Type::i64;
        addition->set_inputs(offset_register, immediate);
        addition->set_outputs(sum);
        sum->set_op(std::move(addition));
    }

    // 3. set lsb to zero (every valid rv64 instruction is at least 2 byte aligned)
    // 3.1 load bitmask
    SSAVar *bit_mask = load_immediate(bb, (int64_t)-2, ip, false);
    // 3.2 apply mask
    SSAVar *jump_addr = bb->add_var(Type::i64, ip);
    {
        auto and_op = std::make_unique<Operation>(Instruction::_and);
        and_op->lifter_info.in_op_size = Type::i64;
        and_op->set_inputs(sum, bit_mask);
        and_op->set_outputs(jump_addr);
        jump_addr->set_op(std::move(and_op));
    }

    // create the jump operation
    // According to the risc-v manual, ijumps which write the ip to x1 (or ra) are considered subroutine calls.
    CfOp &cf_operation = bb->add_cf_op((optimizations & OPT_CALL_RET) && is_link_reg(instr.instr.rd) ? CFCInstruction::icall : CFCInstruction::ijump, nullptr, ip);

    // set operation in- and outputs
    cf_operation.set_inputs(jump_addr);

    if (instr.instr.rd != ZERO_IDX) {
        // the return value address is encoded as immediate
        SSAVar *return_immediate = load_immediate(bb, (int64_t)next_addr, ip, false);

        // write SSAVar of the result of the operation back to mapping
        write_to_mapping(mapping, return_immediate, instr.instr.rd);
    }
}
