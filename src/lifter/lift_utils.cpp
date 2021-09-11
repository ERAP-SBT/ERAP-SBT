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

SSAVar *Lifter::get_from_mapping(BasicBlock *bb, reg_map &mapping, uint64_t reg_id, uint64_t ip, bool is_floating_point_register) {
    if (!is_floating_point_register && reg_id == ZERO_IDX) {
        // return constant zero
        return bb->add_var_imm(0, ip);
    }

    assert(floating_point_support || !is_floating_point_register);

    return mapping[reg_id + (is_floating_point_register ? START_IDX_FLOATING_POINT_STATICS : 0)];
}

void Lifter::write_to_mapping(reg_map &mapping, SSAVar *var, uint64_t reg_id, bool is_floating_point_register) {
    if (!is_floating_point_register && reg_id == ZERO_IDX) {
        return;
    }

    assert(floating_point_support || !is_floating_point_register);

    const uint64_t actual_reg_id = reg_id + (is_floating_point_register ? START_IDX_FLOATING_POINT_STATICS : 0);

    std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = actual_reg_id;
    mapping[actual_reg_id] = var;
}

void Lifter::zero_extend_all_f32(BasicBlock *bb, reg_map &mapping, uint64_t ip) const {
    for (size_t i = 0; i < count_used_static_vars; i++) {
        if (i == ZERO_IDX) {
            continue;
        }

        if (mapping[i]->type == Type::f32) {
            SSAVar *extended_var = bb->add_var(Type::f64, ip);
            auto op = std::make_unique<Operation>(Instruction::zero_extend);
            op->set_inputs(mapping[i]);
            op->set_outputs(extended_var);
            extended_var->set_op(std::move(op));
            std::get<SSAVar::LifterInfo>(extended_var->lifter_info).static_id = i;
            mapping[i] = extended_var;
        }
    }
}

SSAVar *Lifter::get_from_mapping_and_shrink(BasicBlock *bb, reg_map &mapping, uint64_t reg_id, uint64_t ip, const Type expected_type) {
    SSAVar *value = get_from_mapping(bb, mapping, reg_id, ip, is_float(expected_type));
    const int cast_dir_res = cast_dir(expected_type, value->type);

    if (cast_dir_res == -1) {
        std::cerr << "value->type = " << value->type << "\n";
        std::cerr << "expected_type = " << expected_type << "\n";
        assert(0 && "The variable cannot be casted to the expected type!");
    }

    if (cast_dir_res == 1) {
        SSAVar *const casted_value = bb->add_var(expected_type, ip);
        auto op = std::make_unique<Operation>(Instruction::cast);
        op->set_inputs(value);
        op->set_outputs(casted_value);
        casted_value->set_op(std::move(op));
        value = casted_value;
    }
    return value;
}
