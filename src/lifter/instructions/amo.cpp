#include "lifter/lifter.h"

using namespace lifter::RV64;

void load_rs1_to_rd(BasicBlock *bb, const RV64Inst &instr, Lifter::reg_map &mapping, uint64_t ip, const Type &op_size) {
    // to use the standard load command
    RV64Inst mod_instr = instr;
    mod_instr.instr.imm = 0;

    // delegate lifting to standard load
    Lifter::lift_load(bb, mod_instr, mapping, ip, op_size, true);
}

void store_val_to_rs1(BasicBlock *bb, const RV64Inst &instr, Lifter::reg_map &mapping, uint64_t ip, const Type &op_size, SSAVar *value) {
    // to use the standard store command, clear the immediate offset and emulate a value in register rs2
    RV64Inst mod_instr = instr;
    mod_instr.instr.imm = 0;

    SSAVar *real_rs2_val = Lifter::get_from_mapping(bb, mapping, mod_instr.instr.rs2, ip, false);
    Lifter::write_to_mapping(mapping, value, mod_instr.instr.rs2, false);

    // delegate lifting to standard store
    Lifter::lift_store(bb, mod_instr, mapping, ip, op_size);

    // restore register map
    Lifter::write_to_mapping(mapping, real_rs2_val, mod_instr.instr.rs2, false);
}

void Lifter::lift_amo_load_reserve(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) { load_rs1_to_rd(bb, instr, mapping, ip, op_size); }

void Lifter::lift_amo_store_conditional(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    store_val_to_rs1(bb, instr, mapping, ip, op_size, get_from_mapping(bb, mapping, instr.instr.rs2, ip, false));

    // if the operation succeeds (which it always does without actual atomic operations), 0 is placed into the destination register
    write_to_mapping(mapping, bb->add_var_imm(0, ip, false, instr.instr.rd), instr.instr.rd, false);
}

void Lifter::lift_amo_add(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    SSAVar *in_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, false);

    load_rs1_to_rd(bb, instr, mapping, ip, op_size);

    // input1: rd, input2: rs2
    SSAVar *in_1 = get_from_mapping(bb, mapping, instr.instr.rd, ip, false);

    // apply the binary operation
    SSAVar *op_result = bb->add_var(Type::i64, ip);
    {
        auto add_op = std::make_unique<Operation>(Instruction::add);
        add_op->set_inputs(in_1, in_2);
        add_op->set_outputs(op_result);
        op_result->set_op(std::move(add_op));
    }

    // sign extension for the operation result is not requried
    store_val_to_rs1(bb, instr, mapping, ip, op_size, op_result);
}

void Lifter::lift_amo_swap(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    // input1: rd, input2: rs2
    // this is up here so that when rs2 == rd, the call to load_rs1_to_rd doesn't override the value in rs2 as well
    SSAVar *in_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, false);

    load_rs1_to_rd(bb, instr, mapping, ip, op_size);

    // the swap actually only loads the value from memory to rd and stores rs2 in memory
    store_val_to_rs1(bb, instr, mapping, ip, op_size, in_2);
}

void Lifter::lift_amo_xor(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    SSAVar *in_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, false);

    load_rs1_to_rd(bb, instr, mapping, ip, op_size);

    // input1: rd, input2: rs2
    SSAVar *in_1 = get_from_mapping(bb, mapping, instr.instr.rd, ip, false);

    // apply the binary operation
    SSAVar *op_result = bb->add_var(Type::i64, ip);
    {
        auto add_op = std::make_unique<Operation>(Instruction::_xor);
        add_op->set_inputs(in_1, in_2);
        add_op->set_outputs(op_result);
        op_result->set_op(std::move(add_op));
    }

    // sign extension for the operation result is not requried
    store_val_to_rs1(bb, instr, mapping, ip, op_size, op_result);
}

void Lifter::lift_amo_or(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    SSAVar *in_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, false);

    load_rs1_to_rd(bb, instr, mapping, ip, op_size);

    // input1: rd, input2: rs2
    SSAVar *in_1 = get_from_mapping(bb, mapping, instr.instr.rd, ip, false);

    // apply the binary operation
    SSAVar *op_result = bb->add_var(Type::i64, ip);
    {
        auto add_op = std::make_unique<Operation>(Instruction::_or);
        add_op->set_inputs(in_1, in_2);
        add_op->set_outputs(op_result);
        op_result->set_op(std::move(add_op));
    }

    // sign extension for the operation result is not requried
    store_val_to_rs1(bb, instr, mapping, ip, op_size, op_result);
}

void Lifter::lift_amo_and(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    SSAVar *in_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, false);

    load_rs1_to_rd(bb, instr, mapping, ip, op_size);

    // input1: rd, input2: rs2
    SSAVar *in_1 = get_from_mapping(bb, mapping, instr.instr.rd, ip, false);

    // apply the binary operation
    SSAVar *op_result = bb->add_var(Type::i64, ip);
    {
        auto add_op = std::make_unique<Operation>(Instruction::_and);
        add_op->set_inputs(in_1, in_2);
        add_op->set_outputs(op_result);
        op_result->set_op(std::move(add_op));
    }

    // sign extension for the operation result is not requried
    store_val_to_rs1(bb, instr, mapping, ip, op_size, op_result);
}

void Lifter::lift_amo_min_max(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, const uint64_t ip, const Type op_size, const Instruction instr_type) {
    SSAVar *cmp2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip, false);

    load_rs1_to_rd(bb, instr, mapping, ip, op_size);

    SSAVar *cmp1 = get_from_mapping(bb, mapping, instr.instr.rd, ip, false);

    SSAVar *op_result = bb->add_var(op_size, ip);
    {
        auto op = std::make_unique<Operation>(instr_type);
        op->set_inputs(cmp1, cmp2);
        op->set_outputs(op_result);
        op_result->set_op(std::move(op));
    }

    store_val_to_rs1(bb, instr, mapping, ip, op_size, op_result);
}

void Lifter::lift_amo_min(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, const uint64_t ip, const Type op_size, const bool _signed) {
    lift_amo_min_max(bb, instr, mapping, ip, op_size, _signed ? Instruction::min : Instruction::umin);
}

void Lifter::lift_amo_max(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, const uint64_t ip, const Type op_size, const bool _signed) {
    lift_amo_min_max(bb, instr, mapping, ip, op_size, _signed ? Instruction::max : Instruction::umax);
}
