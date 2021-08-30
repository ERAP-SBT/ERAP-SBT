#include <lifter/lifter.h>

using namespace lifter::RV64;

SSAVar *Lifter::load_immediate(BasicBlock *bb, int32_t imm, uint64_t ip, bool binary_relative) {
    SSAVar *input_imm = bb->add_var_imm(imm, ip, binary_relative);
    return input_imm;
}

SSAVar *Lifter::load_immediate(BasicBlock *bb, int64_t imm, uint64_t ip, bool binary_relative) {
    SSAVar *input_imm = bb->add_var_imm(imm, ip, binary_relative);
    return input_imm;
}

std::string Lifter::str_decode_instr(const FrvInst *instr) {
    char str[16];
    frv_format(instr, 16, str);
    return std::string(str);
}

void Lifter::print_invalid_op_size(const Instruction instructionType, const RV64Inst &instr) {
    std::stringstream str;
    str << "Encountered " << instructionType << " instruction with invalid operand size: " << str_decode_instr(&instr.instr);
    DEBUG_LOG(str.str());
}

BasicBlock *Lifter::get_bb(uint64_t addr) const { return ir->bb_at_addr(addr); }

SSAVar *Lifter::shrink_var(BasicBlock *bb, SSAVar *var, uint64_t ip, const Type target_size) {
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

std::optional<SSAVar *> Lifter::convert_type(BasicBlock *bb, uint64_t ip, SSAVar *var, const Type desired_type) {
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

/**
 * Returns the corrosponding value from the mapping: If `is_floating_point_register == true` the slots for the floating points are accessed, if not the slots for the general purpose/integer registers
 * are accessed. As identifer the register index as used by the RISC-V manual and frvdec is used. If the x0-Register is specified (`reg_id = 0 && is_floating_point_register == false`) a 0 immediate is
 * created in the current basic block.
 *
 * @param bb The current {@link BasicBlock basic block}.
 * @param mapping The {@link reg_map mapping} to read from.
 * @param reg_id The identifier (id) of the register as used by RISC-V manual and frvdec.
 * @param ip The current instruction pointer (ip), used for variable creation. (see handling of x0-Register)
 * @param is_floating_point_register Defines from which slots should be read: either general purpose/integer register or floating point slots.
 * @return SSAVar * A pointer to the variable stored at the given position in the mapping.
 */
SSAVar *Lifter::get_from_mapping(BasicBlock *bb, reg_map &mapping, uint64_t reg_id, uint64_t ip, bool is_floating_point_register) {
    if (!is_floating_point_register && reg_id == ZERO_IDX) {
        // return constant zero
        return bb->add_var_imm(0, ip);
    }

    return mapping[reg_id + (is_floating_point_register ? START_IDX_FLOATING_POINT_STATICS : 0)];
}

/**
 * Writes the given {@link SSAVar variable} to the {@link reg_map mapping}. Calls with `reg_id = 0 && is_floating_point_register == false` are ignored due to this are writes the unused slot in the
 * mapping for the x0-Register. With `is_floating_point_register == true` the variable is written to the slots for the floating point registers which are currently stored with a defined @link
 * START_IDX_FLOATING_POINT_STATICS offset @endlink after the integer register variables.
 *
 * @param mapping The {@link reg_map mapping} which should be modified.
 * @param var The variable to write to the mapping.
 * @param reg_id The identifier (id) of the register as used by RISC-V manual and frvdec.
 * @param is_floating_point_register Defines whether the variable should be written to the slots for general puropse/integer register or to the slots for floating point register.
 */
void Lifter::write_to_mapping(reg_map &mapping, SSAVar *var, uint64_t reg_id, bool is_floating_point_register) {
    if (!is_floating_point_register && reg_id == ZERO_IDX) {
        return;
    }
    uint64_t actual_reg_id = reg_id + (is_floating_point_register ? START_IDX_FLOATING_POINT_STATICS : 0);

    std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = actual_reg_id;
    mapping[actual_reg_id] = var;
}
