#include <lifter/lifter.h>

using namespace lifter::RV64;

SSAVar *Lifter::get_csr(reg_map &mapping, uint32_t csr_identifier) {
    // TODO: implement correctly, e.g. for fcsr

    // prevent build warnings
    (void) mapping;
    (void) csr_identifier;
    return nullptr;
}

void Lifter::write_csr(reg_map &mapping, SSAVar *new_csr, uint32_t csr_identifier) {
    // TODO: implement correctly, e.g. for fcsr
    
    // prevent build warnings
    (void) mapping;
    (void) new_csr;
    (void) csr_identifier;
}

void Lifter::lift_csr_read_write(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate) {
    // move the crs value to the dest "register"
    // dont't read csr register if rd is x0
    if (instr.instr.rd != 0) {
        // the identifier for the csr is in the immediate field
        SSAVar *csr = get_csr(mapping, instr.instr.imm);
        write_to_mapping(mapping, csr, instr.instr.rd);
    }

    // if the instruction contains an immediate, the immediate is encoded in the rs1 field
    SSAVar *new_csr = with_immediate ? bb->add_var_imm(instr.instr.rs1, ip) : get_from_mapping(bb, mapping, instr.instr.rs1, ip);
    write_csr(mapping, new_csr, instr.instr.imm);
}

void Lifter::lift_csr_read_set(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate) {
    // move the crs value to the dest "register"
    // the identifier for the csr is in the immediate field
    SSAVar *csr = get_csr(mapping, instr.instr.imm);
    write_to_mapping(mapping, csr, instr.instr.rd);

    // dont't write to the csr if the immediate is zero or the register is x0, in both cases rs1 is zero
    if (instr.instr.rs1 == 0) {

        // TODO:
        // Note that if rs1
        // specifies a register holding a zero value other than x0, the instruction will still attempt to write
        // the unmodified value back to the CSR and will cause any attendant side effects.

        // set all bits as specified by the mask
        SSAVar *rs1 = with_immediate ? bb->add_var_imm(instr.instr.rs1, ip) : get_from_mapping(bb, mapping, instr.instr.rs1, ip);
        SSAVar *new_csr = bb->add_var(Type::i64, ip);

        auto op = std::make_unique<Operation>(Instruction::_or);
        op->set_inputs(csr, rs1);
        op->set_outputs(new_csr);
        new_csr->set_op(std::move(op));
        write_csr(mapping, new_csr, instr.instr.imm);
    }
}

void Lifter::lift_csr_read_clear(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate) {
    // move the crs value to the dest "register"
    // the identifier for the csr is in the immediate field
    SSAVar *csr = get_csr(mapping, instr.instr.imm);
    write_to_mapping(mapping, csr, instr.instr.rd);

    // dont't write to the csr if the immediate is zero or the register is x0, in both cases rs1 is zero
    if (instr.instr.rs1 == 0) {

        // TODO:
        // Note that if rs1
        // specifies a register holding a zero value other than x0, the instruction will still attempt to write
        // the unmodified value back to the CSR and will cause any attendant side effects.

        // negate rs1
        SSAVar *negated_rs1;
        if (with_immediate) {
            // negate the immediate
            negated_rs1 = bb->add_var_imm(~((uint64_t)(instr.instr.rs1)), ip);
        } else {
            negated_rs1 = bb->add_var(Type::i64, ip);
            SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
            auto op = std::make_unique<Operation>(Instruction::_not);
            op->set_inputs(rs1);
            op->set_outputs(negated_rs1);
            negated_rs1->set_op(std::move(op));
        }

        // clear all bits as specified by the mask
        SSAVar *new_csr = bb->add_var(Type::i64, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->set_inputs(csr, negated_rs1);
            op->set_outputs(new_csr);
            new_csr->set_op(std::move(op));
        }

        write_csr(mapping, new_csr, instr.instr.imm);
    }
}