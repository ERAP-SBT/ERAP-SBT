#include <frvdec.h>
#include <lifter/lifter.h>

using namespace lifter::RV64;

SSAVar *zero_extend_csr(BasicBlock *bb, SSAVar *csr, uint64_t ip) {
    SSAVar *extended_csr = bb->add_var(Type::i64, ip);
    auto op = std::make_unique<Operation>(Instruction::zero_extend);
    op->lifter_info.in_op_size = csr->type;
    op->set_inputs(csr);
    op->set_outputs(extended_csr);
    extended_csr->set_op(std::move(op));
    return extended_csr;
}

SSAVar *Lifter::get_csr(reg_map &mapping, uint32_t csr_identifier) {
    // return the var of the fcsr register with id 3
    switch (csr_identifier) {
    case 1:
    case 2:
    case 3:
        assert(floating_point_support && "Please activate the floating point support!");
        return mapping[FCSR_IDX];
    default:
        // stop lifting if status register is not implemented
        std::stringstream str{};
        str << "Implement more control and status registers!\n csr_identifier = " << csr_identifier << "\n";
        DEBUG_LOG(str.str());

        assert(0);
        exit(1);
        break;
    }

    return nullptr;
}

void Lifter::write_csr(reg_map &mapping, SSAVar *new_csr, uint32_t csr_identifier) {
    // return the var of the fcsr register with id 3
    switch (csr_identifier) {
    case 1:
    case 2:
    case 3:
        assert(floating_point_support && "Please activate the floating point support!");
        mapping[FCSR_IDX] = new_csr;
        break;
    default:
        // stop lifting if status register is not implemented
        std::stringstream str{};
        str << "Implement more control and status registers!\n csr_identifier = " << csr_identifier << "\n";
        DEBUG_LOG(str.str());
        assert(0);
        exit(1);
    }
}

void Lifter::lift_csr_read_write(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate) {
    // move the crs value to the dest "register"
    // dont't read csr register if rd is x0
    if (instr.instr.rd != 0) {
        // the identifier for the csr is in the immediate field
        SSAVar *const csr = zero_extend_csr(bb, get_csr(mapping, instr.instr.imm), ip);
        write_to_mapping(mapping, csr, instr.instr.rd);
    }

    // if the instruction contains an immediate, the immediate is encoded in the rs1 field
    SSAVar *new_csr;
    if (with_immediate) {
        new_csr = shrink_var(bb, bb->add_var_imm(instr.instr.rs1, ip), ip, Type::i32);
    } else {
        new_csr = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, Type::i32);
    }

    SSAVar *mask = bb->add_var_imm(0xE0, ip);
    // clear the excpetion flags
    SSAVar *masked_new_csr = bb->add_var(Type::i32, ip);
    {
        auto op = std::make_unique<Operation>(Instruction::_and);
        op->set_inputs(new_csr, mask);
        op->set_outputs(masked_new_csr);
        masked_new_csr->set_op(std::move(op));
    }
    write_csr(mapping, masked_new_csr, instr.instr.imm);
}

void Lifter::lift_csr_read_set(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate) {
    // move the crs value to the dest "register"
    // the identifier for the csr is in the immediate field
    SSAVar *const csr = get_csr(mapping, instr.instr.imm);
    write_to_mapping(mapping, zero_extend_csr(bb, csr, ip), instr.instr.rd);

    // dont't write to the csr if the immediate is zero or the register is x0, in both cases rs1 is zero
    if (instr.instr.rs1 != 0) {

        // TODO:
        // Note that if rs1
        // specifies a register holding a zero value other than x0, the instruction will still attempt to write
        // the unmodified value back to the CSR and will cause any attendant side effects.

        // set all bits as specified by the mask
        SSAVar *rs1 = with_immediate ? bb->add_var_imm(instr.instr.rs1, ip) : get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, Type::i32);
        SSAVar *new_csr = bb->add_var(Type::i32, ip);

        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->lifter_info.in_op_size = Type::i64;
            op->set_inputs(csr, rs1);
            op->set_outputs(new_csr);
            new_csr->set_op(std::move(op));
        }

        SSAVar *mask = bb->add_var_imm(0xE0, ip);
        // clear the excpetion flags
        SSAVar *masked_new_csr = bb->add_var(Type::i32, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->set_inputs(new_csr, mask);
            op->set_outputs(masked_new_csr);
            masked_new_csr->set_op(std::move(op));
        }
        write_csr(mapping, masked_new_csr, instr.instr.imm);
    }
}

void Lifter::lift_csr_read_clear(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate) {
    // move the crs value to the dest "register"
    // the identifier for the csr is in the immediate field
    SSAVar *const csr = get_csr(mapping, instr.instr.imm);
    write_to_mapping(mapping, zero_extend_csr(bb, csr, ip), instr.instr.rd);

    // dont't write to the csr if the immediate is zero or the register is x0, in both cases rs1 is zero
    if (instr.instr.rs1 != 0) {

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
            negated_rs1 = bb->add_var(Type::i32, ip);
            SSAVar *rs1 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, Type::i32);
            auto op = std::make_unique<Operation>(Instruction::_not);
            op->lifter_info.in_op_size = Type::i64;
            op->set_inputs(rs1);
            op->set_outputs(negated_rs1);
            negated_rs1->set_op(std::move(op));
        }

        // clear all bits as specified by the mask
        SSAVar *new_csr = bb->add_var(Type::i32, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->lifter_info.in_op_size = Type::i64;
            op->set_inputs(csr, negated_rs1);
            op->set_outputs(new_csr);
            new_csr->set_op(std::move(op));
        }

        write_csr(mapping, new_csr, instr.instr.imm);
    }
}
