#include "lifter/lifter.h"

using namespace lifter::RV64;

SSAVar *Lifter::load_rs1_to_rd(BasicBlock *bb, const RV64Inst &instr, Lifter::reg_map &mapping, uint64_t ip, const Type op_size) {
    // no usage of "normal" load function in order to optimize

    // get the address value
    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, (int64_t)ip);

    // create SSAVariable for the destination operand
    SSAVar *load_dest = bb->add_var(op_size, ip);

    // create the load operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::load);

    operation->set_inputs(rs1, mapping[MEM_IDX]);
    operation->set_outputs(load_dest);

    // assign the operation as variable of the destination
    load_dest->set_op(std::move(operation));

    SSAVar *write_back_result = load_dest;

    // extend the result ot 64bit to store it in the mapping (according to the definition of the amo instructions)
    if (op_size == Type::i32) {
        SSAVar *extended_result = bb->add_var(Type::i64, ip);
        {
            auto extend_operation = std::make_unique<Operation>(Instruction::sign_extend);
            extend_operation->set_inputs(load_dest);
            extend_operation->set_outputs(extended_result);
            extended_result->set_op(std::move(extend_operation));
        }
        write_back_result = extended_result;
    }

    // write the [extended, if op_size == Type::i32] result to the mapping
    write_to_mapping(mapping, write_back_result, instr.instr.rd);

    // but return the [not extended] result. With op_size == Type::i32, we perform 32bit operations and therefore can use the not sign extended result.
    // And with op_size == Type::i64 the result is returned.
    return load_dest;
}

void Lifter::store_val_to_rs1(BasicBlock *bb, const RV64Inst &instr, Lifter::reg_map &mapping, uint64_t ip, const Type &op_size, SSAVar *value) {
    // to use the standard store command, clear the immediate offset and emulate a value in register rs2
    RV64Inst mod_instr = instr;
    mod_instr.instr.imm = 0;

    SSAVar *real_rs2_val = get_from_mapping(bb, mapping, mod_instr.instr.rs2, ip);
    write_to_mapping(mapping, value, mod_instr.instr.rs2);

    // delegate lifting to standard store
    lift_store(bb, mod_instr, mapping, ip, op_size);

    // restore register map
    write_to_mapping(mapping, real_rs2_val, mod_instr.instr.rs2);
}

void Lifter::lift_amo_load_reserve(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) { load_rs1_to_rd(bb, instr, mapping, ip, op_size); }

void Lifter::lift_amo_store_conditional(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    store_val_to_rs1(bb, instr, mapping, ip, op_size, get_from_mapping(bb, mapping, instr.instr.rs2, ip));

    // if the operation succeeds (which it always does without actual atomic operations), 0 is placed into the destination register
    write_to_mapping(mapping, bb->add_var_imm(0, ip, false, instr.instr.rd), instr.instr.rd);
}

void Lifter::lift_amo_swap(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    // input1: rd, input2: rs2
    // this is up here so that when rs2 == rd, the call to load_rs1_to_rd doesn't override the value in rs2 as well
    SSAVar *in_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip);

    load_rs1_to_rd(bb, instr, mapping, ip, op_size);

    // the swap actually only loads the value from memory to rd and stores rs2 in memory
    store_val_to_rs1(bb, instr, mapping, ip, op_size, in_2);
}

void Lifter::lift_amo_binary_op(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    SSAVar *in_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip);

    // input1: rd, input2: rs2
    SSAVar *in_1 = load_rs1_to_rd(bb, instr, mapping, ip, op_size);

    if (in_2->type != op_size) {
        in_2 = shrink_var(bb, in_2, ip, Type::i32);
    }

    // apply the binary operation
    SSAVar *op_result = bb->add_var(op_size, ip);
    {
        auto op = std::make_unique<Operation>(instruction_type);
        op->set_inputs(in_1, in_2);
        op->set_outputs(op_result);
        op_result->set_op(std::move(op));
    }

    // sign extension for the operation result is not requried
    store_val_to_rs1(bb, instr, mapping, ip, op_size, op_result);
}
