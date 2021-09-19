#include "lifter/lifter.h"

using namespace lifter::RV64;

void Lifter::lift_load(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size, bool sign_extend) {
    SSAVar *load_addr;

    if (instr.instr.imm != 0) {
        // 1. load offset
        SSAVar *offset = load_immediate(bb, instr.instr.imm, ip, false);

        // 3. add offset to rs1
        SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, (int64_t)ip);
        load_addr = bb->add_var(Type::i64, ip);
        {
            auto add_op = std::make_unique<Operation>(Instruction::add);
            add_op->lifter_info.in_op_size = Type::i64;
            add_op->set_inputs(rs1, offset);
            add_op->set_outputs(load_addr);
            load_addr->set_op(std::move(add_op));
        }
    } else {
        load_addr = get_from_mapping(bb, mapping, instr.instr.rs1, (int64_t)ip);
    }

    // create SSAVariable for the destination operand
    SSAVar *load_dest = bb->add_var(op_size, ip);

    // create the load operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::load);
    operation->lifter_info.in_op_size = Type::i64;

    operation->set_inputs(load_addr, mapping[MEM_IDX]);
    operation->set_outputs(load_dest);

    // assign the operation as variable of the destination
    load_dest->set_op(std::move(operation));

    // last step: extend load_dest variable to 64 bit
    if (cast_dir(op_size, Type::i64) == 1) {
        SSAVar *extended_result = bb->add_var(Type::i64, ip);
        {
            auto extend_operation = std::make_unique<Operation>((sign_extend ? Instruction::sign_extend : Instruction::zero_extend));
            extend_operation->lifter_info.in_op_size = op_size;
            extend_operation->set_inputs(load_dest);
            extend_operation->set_outputs(extended_result);
            extended_result->set_op(std::move(extend_operation));
        }
        load_dest = extended_result;
    }

    // write SSAVar of the result of the operation and new memory token back to mapping
    write_to_mapping(mapping, load_dest, instr.instr.rd, is_float(op_size));
}

void Lifter::lift_store(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size) {
    SSAVar *store_addr;
    if (instr.instr.imm != 0) {
        // 1. load offset
        SSAVar *offset = load_immediate(bb, instr.instr.imm, ip, false);

        // 2. add offset to rs1
        SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
        store_addr = bb->add_var(Type::i64, ip);
        {
            auto add_op = std::make_unique<Operation>(Instruction::add);
            add_op->lifter_info.in_op_size = Type::i64;
            add_op->set_inputs(rs1, offset);
            add_op->set_outputs(store_addr);
            store_addr->set_op(std::move(add_op));
        }
    } else {
        store_addr = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
    }

    // cast variable to store to operand size
    SSAVar *const rs2 = get_from_mapping_and_shrink(bb, mapping, instr.instr.rs2, ip, op_size);

    // create memory_token
    SSAVar *result_memory_token = bb->add_var(Type::mt, ip, MEM_IDX);

    // create the store operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::store);
    operation->lifter_info.in_op_size = op_size;

    // set in- and outputs
    operation->set_inputs(store_addr, rs2, mapping[MEM_IDX]);
    operation->set_outputs(result_memory_token);

    // set operation
    result_memory_token->set_op(std::move(operation));

    // write memory_token back
    write_to_mapping(mapping, result_memory_token, MEM_IDX);
}
