#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_mul(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instr_type, const Type in_type) {
    // assign the first input and cast it to the correct size if necessary
    SSAVar *rs_1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
    if (rs_1->type != in_type) {
        auto cast = convert_type(bb, ip, rs_1, in_type);
        if (cast.has_value()) {
            rs_1 = cast.value();
        } else {
            print_invalid_op_size(instr_type, instr);
        }
    }

    // assign the second input and cast if necessary
    SSAVar *rs_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip);
    if (rs_2->type != in_type) {
        auto cast = convert_type(bb, ip, rs_2, in_type);
        if (cast.has_value()) {
            rs_2 = cast.value();
        } else {
            print_invalid_op_size(instr_type, instr);
        }
    }

    // create operation destination and the main operation
    SSAVar *dest = bb->add_var(in_type, ip);
    {
        auto op = std::make_unique<Operation>(instr_type);
        op->set_inputs(rs_1, rs_2);
        op->set_outputs(dest);
        dest->set_op(std::move(op));
    }

    // sign extend result to 64-bit register if the instruction was mul_w
    if (in_type == Type::i32) {
        SSAVar *new_dest = bb->add_var(Type::i64, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::sign_extend);
            op->set_inputs(dest);
            op->set_outputs(new_dest);
            new_dest->set_op(std::move(op));
        }
        dest = new_dest;
    }

    // write SSAVar of the result of the operation back to mapping
    write_to_mapping(mapping, dest, instr.instr.rd);
}

void Lifter::lift_div(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool _signed, bool remainder, const Type in_type) {
    // assign the first input and cast it to the correct size if necessary
    SSAVar *rs_1 = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
    if (rs_1->type != in_type) {
        auto cast = convert_type(bb, ip, rs_1, in_type);
        if (cast.has_value()) {
            rs_1 = cast.value();
        } else {
            print_invalid_op_size(_signed ? Instruction::div : Instruction::udiv, instr);
        }
    }

    // assign the second input and cast if necessary
    SSAVar *rs_2 = get_from_mapping(bb, mapping, instr.instr.rs2, ip);
    if (rs_2->type != in_type) {
        auto cast = convert_type(bb, ip, rs_2, in_type);
        if (cast.has_value()) {
            rs_2 = cast.value();
        } else {
            print_invalid_op_size(_signed ? Instruction::div : Instruction::udiv, instr);
        }
    }

    // create operation destination and the main operation
    SSAVar *dest = bb->add_var(in_type, ip);
    {
        auto op = std::make_unique<Operation>(_signed ? Instruction::div : Instruction::udiv);
        op->set_inputs(rs_1, rs_2);
        op->set_outputs(remainder ? nullptr : dest, remainder ? dest : nullptr);
        dest->set_op(std::move(op));
    }

    // sign extend result to 64-bit register if the instruction was mul_w
    if (in_type == Type::i32) {
        SSAVar *new_dest = bb->add_var(Type::i64, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::sign_extend);
            op->set_inputs(dest);
            op->set_outputs(new_dest);
            new_dest->set_op(std::move(op));
        }
        dest = new_dest;
    }

    // write SSAVar of the result of the operation back to mapping
    write_to_mapping(mapping, dest, instr.instr.rd);
}
