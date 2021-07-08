#include <lifter/lifter.h>

using namespace lifter::RV64;

SSAVar *Lifter::load_immediate(BasicBlock *bb, int32_t imm, uint64_t ip, bool binary_relative, size_t reg) {
    SSAVar *input_imm = bb->add_var_imm(imm, ip, binary_relative, reg);
    return input_imm;
}

SSAVar *Lifter::load_immediate(BasicBlock *bb, int64_t imm, uint64_t ip, bool binary_relative, size_t reg) {
    SSAVar *input_imm = bb->add_var_imm(imm, ip, binary_relative, reg);
    return input_imm;
}

std::string Lifter::str_decode_instr(const FrvInst *instr) {
    char str[16];
    frv_format(instr, 16, str);
    return std::string(str);
}

void Lifter::print_invalid_op_size(const Instruction &instructionType, RV64Inst &instr) {
    std::stringstream str;
    str << "Encountered " << instructionType << " instruction with invalid operand size: " << str_decode_instr(&instr.instr);
    DEBUG_LOG(str.str());
}

BasicBlock *Lifter::get_bb(uint64_t addr) const {
    for (auto &bb_ptr : ir->basic_blocks) {
        const uint64_t virt_start_addr = bb_ptr->virt_start_addr;
        const uint64_t virt_end_addr = bb_ptr->virt_end_addr;

        // only the dummy basic block should have the start address 0x0 and we don't want to return the dummy bb
        if (virt_start_addr) {
            // either the basic block is already parsed and the jmp address is in the parsed range OR the basic block should be parsed soon and the jmp address is the start address
            if ((virt_end_addr && virt_start_addr <= addr && virt_end_addr >= addr) || (!virt_end_addr && addr == virt_start_addr)) {
                return bb_ptr.get();
            }
        }
    }

    return nullptr;
}

SSAVar *Lifter::shrink_var(BasicBlock *bb, SSAVar *var, uint64_t ip, const Type &target_size) {
    // create cast operation
    std::unique_ptr<Operation> cast = std::make_unique<Operation>(Instruction::cast);

    // set in- and outputs
    cast->set_inputs(var);

    // create casted variable
    SSAVar *destination = bb->add_var(target_size, ip);
    cast->set_outputs(destination);
    destination->set_op(std::move(cast));

    return destination;
}

std::optional<SSAVar *> Lifter::convert_type(BasicBlock *bb, uint64_t ip, SSAVar *var, Type desired_type) {
    if (var->type == desired_type || var->type == Type::imm) {
        return var;
    }

    int type_order = cast_dir(var->type, desired_type);
    if (type_order == -1) {
        return std::nullopt;
    }

    SSAVar *new_var = bb->add_var(desired_type, ip);
    std::unique_ptr<Operation> op;
    if (!type_order) {
        op = std::make_unique<Operation>(Instruction::cast);
    } else {
        op = std::make_unique<Operation>(Instruction::sign_extend);
    }
    op->set_inputs(var);
    op->set_outputs(new_var);
    new_var->set_op(std::move(op));
    return new_var;
}

SSAVar *Lifter::get_from_mapping(BasicBlock *bb, reg_map &mapping, int reg_id, int ip) {
    if (reg_id == ZERO_IDX) {
        // return constant zero
        return bb->add_var_imm(0, ip);
    }
    return mapping.at(reg_id);
}

void Lifter::write_to_mapping(reg_map &mapping, SSAVar *var, int reg_id) {
    if (reg_id == ZERO_IDX) {
        return;
    }

    mapping.at(reg_id) = var;
}
