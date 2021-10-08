#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_shift_shared(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size, SSAVar *shift_val) {
    SSAVar *const source = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs1, ip, op_size);

    auto *result = bb->add_var(op_size, ip);
    auto op = std::make_unique<Operation>(instruction_type);
    op->lifter_info.in_op_size = op_size;
    op->set_inputs(source, shift_val);
    op->set_outputs(result);
    result->set_op(std::move(op));

    if (op_size == Type::i32) {
        // produces a sign-extended result
        auto *sext_res = bb->add_var(Type::i64, ip);
        auto op = std::make_unique<Operation>(Instruction::sign_extend);
        op->lifter_info.in_op_size = Type::i32;
        op->set_inputs(result);
        op->set_outputs(sext_res);
        sext_res->set_op(std::move(op));
        result = sext_res;
    }

    write_to_mapping(mapping, result, instr.instr.rd);
}

void Lifter::lift_shift(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // prepare for shift, only use lower 5bits

    SSAVar *mask;

    // cast immediate from 64bit to 32bit if instruction has 32bit size
    if (op_size == Type::i32) {
        mask = load_immediate(bb, (int32_t)0x1F, ip, false);
    } else {
        mask = load_immediate(bb, (int64_t)0x3F, ip, false);
    }

    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip);

    if (rs2->type != op_size) {
        auto *casted_rs2 = bb->add_var(op_size, ip);
        casted_rs2->set_op(Operation::new_cast(casted_rs2, rs2));
        rs2 = casted_rs2;
    }

    // create new variable with the result of the masking
    SSAVar *masked_count_shifts = bb->add_var(op_size, ip);
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::_and);
    operation->lifter_info.in_op_size = Type::i64;
    operation->set_inputs(rs2, mask);
    operation->set_outputs(masked_count_shifts);
    masked_count_shifts->set_op(std::move(operation));

    lift_shift_shared(bb, instr, mapping, ip, instruction_type, op_size, masked_count_shifts);
}

void Lifter::lift_shift_immediate(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // masking the operand
    SSAVar *shift_amount;
    if (op_size == Type::i32) {
        shift_amount = bb->add_var_imm(instr.instr.imm & 0x1F, ip);
    } else {
        shift_amount = bb->add_var_imm(instr.instr.imm & 0x3F, ip);
    }

    lift_shift_shared(bb, instr, mapping, ip, instruction_type, op_size, shift_amount);
}
