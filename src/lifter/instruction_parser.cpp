#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::parse_instruction(RV64Inst instr, BasicBlock *bb, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    switch (instr.instr.mnem) {
    case FRV_INVALID:
        liftInvalid(bb, ip);
        break;

    case FRV_LB:
        lift_load(bb, instr, mapping, ip, Type::i8, true);
        break;
    case FRV_LH:
        lift_load(bb, instr, mapping, ip, Type::i16, true);
        break;
    case FRV_LW:
        lift_load(bb, instr, mapping, ip, Type::i32, true);
        break;
    case FRV_LD:
        lift_load(bb, instr, mapping, ip, Type::i64, true);
        break;
    case FRV_LBU:
        lift_load(bb, instr, mapping, ip, Type::i8, false);
        break;
    case FRV_LHU:
        lift_load(bb, instr, mapping, ip, Type::i16, false);
        break;
    case FRV_LWU:
        lift_load(bb, instr, mapping, ip, Type::i32, false);
        break;

    case FRV_SB:
        lift_store(bb, instr, mapping, ip, Type::i8);
        break;
    case FRV_SH:
        lift_store(bb, instr, mapping, ip, Type::i16);
        break;
    case FRV_SW:
        lift_store(bb, instr, mapping, ip, Type::i32);
        break;
    case FRV_SD:
        lift_store(bb, instr, mapping, ip, Type::i64);
        break;

    case FRV_ADD:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::add, Type::i64);
        break;
    case FRV_ADDW:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::add, Type::i32);
        break;
    case FRV_ADDI:
        lift_arithmetical_logical_immediate(bb, instr, mapping, ip, Instruction::add, Type::i64);
        break;
    case FRV_ADDIW:
        lift_arithmetical_logical_immediate(bb, instr, mapping, ip, Instruction::add, Type::i32);
        break;

    case FRV_MUL:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::mul_l, Type::i64);
        break;
    case FRV_MULH:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::ssmul_h, Type::i64);
        break;
    case FRV_MULHSU:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::sumul_h, Type::i64);
        break;
    case FRV_MULHU:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::uumul_h, Type::i64);
        break;
    case FRV_MULW:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::mul_l, Type::i32);
        break;

    case FRV_DIV:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::div, Type::i64);
        break;
    case FRV_DIVU:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::udiv, Type::i64);
        break;
    case FRV_DIVW:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::div, Type::i32);
        break;
    case FRV_DIVUW:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::udiv, Type::i32);
        break;

    case FRV_REM:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::rem, Type::i64);
        break;
    case FRV_REMU:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::urem, Type::i64);
        break;
    case FRV_REMW:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::rem, Type::i32);
        break;
    case FRV_REMUW:
        lift_mul_div_rem(bb, instr, mapping, ip, Instruction::urem, Type::i32);
        break;

    case FRV_SUB:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::sub, Type::i64);
        break;
    case FRV_SUBW:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::sub, Type::i32);
        break;

    case FRV_AND:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::_and, Type::i64);
        break;
    case FRV_ANDI:
        lift_arithmetical_logical_immediate(bb, instr, mapping, ip, Instruction::_and, Type::i64);
        break;

    case FRV_OR:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::_or, Type::i64);
        break;
    case FRV_ORI:
        lift_arithmetical_logical_immediate(bb, instr, mapping, ip, Instruction::_or, Type::i64);
        break;

    case FRV_XOR:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::_xor, Type::i64);
        break;
    case FRV_XORI:
        lift_arithmetical_logical_immediate(bb, instr, mapping, ip, Instruction::_xor, Type::i64);
        break;

    case FRV_SLL:
        lift_shift(bb, instr, mapping, ip, Instruction::shl, Type::i64);
        break;
    case FRV_SLLW:
        lift_shift(bb, instr, mapping, ip, Instruction::shl, Type::i32);
        break;
    case FRV_SLLI:
        lift_shift_immediate(bb, instr, mapping, ip, Instruction::shl, Type::i64);
        break;
    case FRV_SLLIW:
        lift_shift_immediate(bb, instr, mapping, ip, Instruction::shl, Type::i32);
        break;

    case FRV_SRL:
        lift_shift(bb, instr, mapping, ip, Instruction::shr, Type::i64);
        break;
    case FRV_SRLW:
        lift_shift(bb, instr, mapping, ip, Instruction::shr, Type::i32);
        break;
    case FRV_SRLI:
        lift_shift_immediate(bb, instr, mapping, ip, Instruction::shr, Type::i64);
        break;
    case FRV_SRLIW:
        lift_shift_immediate(bb, instr, mapping, ip, Instruction::shr, Type::i32);
        break;

    case FRV_SRA:
        lift_shift(bb, instr, mapping, ip, Instruction::sar, Type::i64);
        break;
    case FRV_SRAW:
        lift_shift(bb, instr, mapping, ip, Instruction::sar, Type::i32);
        break;
    case FRV_SRAI:
        lift_shift_immediate(bb, instr, mapping, ip, Instruction::sar, Type::i64);
        break;
    case FRV_SRAIW:
        lift_shift_immediate(bb, instr, mapping, ip, Instruction::sar, Type::i32);
        break;

    case FRV_SLTI:
        lift_slt(bb, instr, mapping, ip, false, true);
        break;
    case FRV_SLTIU:
        lift_slt(bb, instr, mapping, ip, true, true);
        break;
    case FRV_SLT:
        lift_slt(bb, instr, mapping, ip, false, false);
        break;
    case FRV_SLTU:
        lift_slt(bb, instr, mapping, ip, true, false);
        break;

    case FRV_FENCE:
    case FRV_FENCEI:
        lift_fence(bb, instr, ip);
        break;

    case FRV_AUIPC:
        lift_auipc(bb, instr, mapping, ip);
        break;

    case FRV_LUI:
        lift_lui(bb, instr, mapping, ip);
        break;

    case FRV_JAL:
        lift_jal(bb, instr, mapping, ip, next_addr);
        break;
    case FRV_JALR:
        lift_jalr(bb, instr, mapping, ip, next_addr);
        break;

    case FRV_BEQ:
    case FRV_BNE:
    case FRV_BLT:
    case FRV_BGE:
    case FRV_BLTU:
    case FRV_BGEU:
        lift_branch(bb, instr, mapping, ip, next_addr);
        break;

    case FRV_ECALL:
        // this also includes the EBREAK
        lift_ecall(bb, mapping, ip, next_addr);
        break;

    default:
        char instr_str[16];
        frv_format(&instr.instr, 16, instr_str);

        std::stringstream str;
        str << "Encountered invalid instruction during lifting: " << instr_str;
        DEBUG_LOG(str.str());

        // TODO: add unreachable instruction
    }
}

inline void Lifter::liftInvalid([[maybe_unused]] BasicBlock *bb, [[maybe_unused]] uint64_t ip) {
#ifdef DEBUG
    std::stringstream str;
    str << "Encountered invalid instruction during lifting. (BasicBlock #0x" << std::hex << bb->id << ", address <0x" << ip << ">)";
    DEBUG_LOG(str.str());
#endif
}

void Lifter::lift_slt(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, bool isUnsigned, bool withImmediate) {
    // get operands for operations (the operands which were compared)
    SSAVar *first_operand = mapping.at(instr.instr.rs1);
    SSAVar *second_operand;

    if (withImmediate) {
        second_operand = bb->add_var_imm(instr.instr.imm, ip);
    } else {
        second_operand = mapping.at(instr.instr.rs2);
    }

    // create variables for result
    SSAVar *one = bb->add_var_imm(1, ip);
    SSAVar *zero = bb->add_var_imm(0, ip);

    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(Type::i64, ip, instr.instr.rd);

    // create slt operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(isUnsigned ? Instruction::sltu : Instruction::slt);

    // set in- and outputs
    operation->set_inputs(first_operand, second_operand, one, zero);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    // write SSAVar of the result of the operation back to mapping
    mapping.at(instr.instr.rd) = destination;
}

void Lifter::lift_auipc(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip) {
    // 1. load the immediate
    SSAVar *immediate = load_immediate(bb, instr.instr.imm, ip);
    // 2. load instruction pointer as immediate
    SSAVar *ip_immediate = load_immediate(bb, (int64_t)ip, ip);

    // 3. add immediate to instruction pointer
    SSAVar *result = bb->add_var(Type::i64, ip, instr.instr.rd);
    {
        auto add_op = std::make_unique<Operation>(Instruction::add);
        add_op->set_inputs(ip_immediate, immediate);
        add_op->set_outputs(result);
        result->set_op(std::move(add_op));
    }
    // write SSAVar back to mapping
    mapping.at(instr.instr.rd) = result;
}

void Lifter::lift_lui(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip) {
    // create the immediate loading operation (with built-in sign extension)
    SSAVar *immediate = load_immediate(bb, (int64_t)instr.instr.imm, ip, instr.instr.rd);

    // write SSAVar back to mapping
    mapping.at(instr.instr.rd) = immediate;
}

void Lifter::lift_fence([[maybe_unused]] BasicBlock *bb, [[maybe_unused]] RV64Inst &instr, [[maybe_unused]] uint64_t ip) {
#ifdef DEBUG
    std::stringstream str;
    str << "Skipping " << str_decode_instr(&instr.instr) << " instruction. (BasicBlock #0x" << std::hex << bb->id << ", address <0x" << ip << ">)";
    DEBUG_LOG(str.str());
#endif
}
