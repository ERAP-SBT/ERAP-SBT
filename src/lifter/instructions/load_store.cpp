#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_load(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type &op_size, bool sign_extend) {
    // 1. load offset
    SSAVar *offset = load_immediate(bb, instr.instr.imm, ip, false);

    // 3. add offset to rs1
    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, (int64_t)ip);
    SSAVar *load_addr = bb->add_var(Type::i64, ip);
    {
        auto add_op = std::make_unique<Operation>(Instruction::add);
        add_op->set_inputs(rs1, offset);
        add_op->set_outputs(load_addr);
        load_addr->set_op(std::move(add_op));
    }

    // create SSAVariable for the destination operand
    SSAVar *load_dest = bb->add_var(op_size, ip);

    // create the load operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::load);

    operation->set_inputs(load_addr, mapping.at(MEM_IDX));
    operation->set_outputs(load_dest);

    // assign the operation as variable of the destination
    load_dest->set_op(std::move(operation));

    // last step: extend load_dest variable to 64 bit
    if (cast_dir(op_size, Type::i64)) {
        SSAVar *extended_result = bb->add_var(Type::i64, ip, instr.instr.rd);
        {
            auto extend_operation = std::make_unique<Operation>((sign_extend ? Instruction::sign_extend : Instruction::zero_extend));
            extend_operation->set_inputs(load_dest);
            extend_operation->set_outputs(extended_result);
            extended_result->set_op(std::move(extend_operation));
        }
        load_dest = extended_result;
    }

    // write SSAVar of the result of the operation and new memory token back to mapping
    write_to_mapping(mapping, load_dest, instr.instr.rd);
}

void Lifter::lift_store(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type &op_size) {
    // 1. load offset
    SSAVar *offset = load_immediate(bb, instr.instr.imm, ip, false);

    // 2. add offset to rs1
    SSAVar *rs1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
    SSAVar *store_addr = bb->add_var(Type::i64, ip);
    {
        auto add_op = std::make_unique<Operation>(Instruction::add);
        add_op->set_inputs(rs1, offset);
        add_op->set_outputs(store_addr);
        store_addr->set_op(std::move(add_op));
    }

    // cast variable to store to operand size
    SSAVar *rs2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip);
    SSAVar *store_var = shrink_var(bb, rs2, ip, op_size);

    // create memory_token
    SSAVar *result_memory_token = bb->add_var(Type::mt, ip, MEM_IDX);

    // create the store operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::store);

    // set in- and outputs
    operation->set_inputs(store_addr, store_var, mapping.at(MEM_IDX));
    operation->set_outputs(result_memory_token);

    // set operation
    result_memory_token->set_op(std::move(operation));

    // write memory_token back
    mapping.at(MEM_IDX) = result_memory_token;
}