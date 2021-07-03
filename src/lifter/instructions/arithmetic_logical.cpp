#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_arithmetical_logical(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction &instruction_type, const Type &op_size) {
    SSAVar *source_one = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
    SSAVar *source_two = get_from_mapping(bb, mapping, instr.instr.rs2, ip);

    // test for invalid operand sizes
    if (source_one->type != op_size) {
        auto cast = convert_type(bb, ip, source_one, op_size);
        if (cast.has_value()) {
            source_one = cast.value();
        } else {
            print_invalid_op_size(instruction_type, instr);
        }
    }
    if (source_two->type != op_size) {
        auto cast = convert_type(bb, ip, source_two, op_size);
        if (cast.has_value()) {
            source_two = cast.value();
        } else {
            print_invalid_op_size(instruction_type, instr);
        }
    }

    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(op_size, ip, instr.instr.rd);

    // create the operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(instruction_type);

    // set operation in- and outputs
    operation->set_inputs(source_one, source_two);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    // write SSAVar of the result of the operation back to mapping
    write_to_mapping(mapping, destination, instr.instr.rd);
}

void Lifter::lift_arithmetical_logical_immediate(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction &instruction_type, const Type &op_size) {
    // create immediate var
    SSAVar *immediate;
    if (op_size == Type::i32) {
        immediate = load_immediate(bb, instr.instr.imm, ip, false);
    } else {
        immediate = load_immediate(bb, (int64_t)instr.instr.imm, ip, false);
    }

    SSAVar *source_one = get_from_mapping(bb, mapping, instr.instr.rs1, ip);

    // test for invalid operand sizes and don't check immediates op_size
    if (!source_one->const_evaluable && source_one->type != op_size) {
        auto cast = convert_type(bb, ip, source_one, op_size);
        if (cast.has_value()) {
            source_one = cast.value();
        } else {
            print_invalid_op_size(instruction_type, instr);
        }
    }

    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(op_size, ip, instr.instr.rd);

    // create the operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(instruction_type);

    // set operation in- and outputs
    operation->set_inputs(source_one, immediate);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    // write SSAVar of the result of the operation back to mapping
    write_to_mapping(mapping, destination, instr.instr.rd);
}
