#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_arithmetical_logical(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    const bool is_floating_point = type_is_floating_point(op_size);
    SSAVar *source_one = get_from_mapping(bb, mapping, instr.instr.rs1, ip, is_floating_point);
    SSAVar *source_two = get_from_mapping(bb, mapping, instr.instr.rs2, ip, is_floating_point);

    // test for invalid operand sizes
    if (!is_floating_point && source_one->type != op_size) {
        auto cast = convert_type(bb, ip, source_one, op_size);
        if (cast.has_value()) {
            source_one = cast.value();
        } else {
            print_invalid_op_size(instruction_type, instr);
        }
    }
    if (!is_floating_point && source_two->type != op_size) {
        auto cast = convert_type(bb, ip, source_two, op_size);
        if (cast.has_value()) {
            source_two = cast.value();
        } else {
            print_invalid_op_size(instruction_type, instr);
        }
    }

    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(op_size, ip);

    // create the operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(instruction_type);

    // set operation in- and outputs
    operation->set_inputs(source_one, source_two);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    if (op_size == Type::i32) {
        // sign extend
        auto *sign_extension = bb->add_var(Type::i64, ip);
        auto op = std::make_unique<Operation>(Instruction::sign_extend);
        op->set_inputs(destination);
        op->set_outputs(sign_extension);
        sign_extension->set_op(std::move(op));

        destination = sign_extension;
    }

    // write SSAVar of the result of the operation back to mapping
    write_to_mapping(mapping, destination, instr.instr.rd, is_floating_point);
}

void Lifter::lift_arithmetical_logical_immediate(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size) {
    // create immediate var
    SSAVar *immediate;
    if (op_size == Type::i32) {
        immediate = load_immediate(bb, instr.instr.imm, ip, false);
    } else {
        immediate = load_immediate(bb, (int64_t)instr.instr.imm, ip, false);
    }

    if (instr.instr.rs1 == 0 && instruction_type == Instruction::add) {
        // just write out the immediate
        write_to_mapping(mapping, immediate, instr.instr.rd);
        return;
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
    SSAVar *destination = bb->add_var(op_size, ip);

    // create the operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(instruction_type);

    // set operation in- and outputs
    operation->set_inputs(source_one, immediate);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    if (op_size == Type::i32) {
        // sign extend
        auto *sign_extension = bb->add_var(Type::i64, ip);
        auto op = std::make_unique<Operation>(Instruction::sign_extend);
        op->set_inputs(destination);
        op->set_outputs(sign_extension);
        sign_extension->set_op(std::move(op));

        destination = sign_extension;
    }

    // write SSAVar of the result of the operation back to mapping
    write_to_mapping(mapping, destination, instr.instr.rd);
}
