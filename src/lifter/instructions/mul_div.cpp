#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift_mul_div_rem(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction &instr_type, const Type &in_type) {
    // assign the first input and cast it to the correct size if necessary
    SSAVar *rs_1 = mapping.at(instr.instr.rs1);
    if (rs_1->type != in_type) {
        auto cast = convert_type(bb, ip, rs_1, in_type);
        if (cast.has_value()) {
            rs_1 = cast.value();
        } else {
            print_invalid_op_size(instr_type, instr);
        }
    }

    // assign the second input and cast if necessary
    SSAVar *rs_2 = mapping.at(instr.instr.rs2);
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
        dest = bb->add_var(Type::i64, ip);
        {
            auto op = std::make_unique<Operation>(Instruction::sign_extend);
            op->set_inputs(mapping.at(instr.instr.rs1));
            op->set_outputs(dest);
            dest->set_op(std::move(op));
        }
    }

    // write SSAVar of the result of the operation back to mapping
    mapping.at(instr.instr.rd) = dest;
}
