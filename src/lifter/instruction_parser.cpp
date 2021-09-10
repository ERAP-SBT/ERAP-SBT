#include "lifter/lifter.h"

using namespace lifter::RV64;

void Lifter::parse_instruction(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    switch (instr.instr.mnem) {
    case FRV_INVALID:
        lift_invalid(bb, ip);
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
        lift_mul(bb, instr, mapping, ip, Instruction::mul_l, Type::i64);
        break;
    case FRV_MULH:
        lift_mul(bb, instr, mapping, ip, Instruction::ssmul_h, Type::i64);
        break;
    case FRV_MULHSU:
        lift_mul(bb, instr, mapping, ip, Instruction::sumul_h, Type::i64);
        break;
    case FRV_MULHU:
        lift_mul(bb, instr, mapping, ip, Instruction::uumul_h, Type::i64);
        break;
    case FRV_MULW:
        lift_mul(bb, instr, mapping, ip, Instruction::mul_l, Type::i32);
        break;

    case FRV_DIV:
        lift_div(bb, instr, mapping, ip, true, false, Type::i64);
        break;
    case FRV_DIVU:
        lift_div(bb, instr, mapping, ip, false, false, Type::i64);
        break;
    case FRV_DIVW:
        lift_div(bb, instr, mapping, ip, true, false, Type::i32);
        break;
    case FRV_DIVUW:
        lift_div(bb, instr, mapping, ip, false, false, Type::i32);
        break;

    case FRV_REM:
        lift_div(bb, instr, mapping, ip, true, true, Type::i64);
        break;
    case FRV_REMU:
        lift_div(bb, instr, mapping, ip, false, true, Type::i64);
        break;
    case FRV_REMW:
        lift_div(bb, instr, mapping, ip, true, true, Type::i32);
        break;
    case FRV_REMUW:
        lift_div(bb, instr, mapping, ip, false, true, Type::i32);
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

    case FRV_LRW:
        lift_amo_load_reserve(bb, instr, mapping, ip, Type::i32);
        break;
    case FRV_LRD:
        lift_amo_load_reserve(bb, instr, mapping, ip, Type::i64);
        break;

    case FRV_SCW:
        lift_amo_store_conditional(bb, instr, mapping, ip, Type::i32);
        break;
    case FRV_SCD:
        lift_amo_store_conditional(bb, instr, mapping, ip, Type::i64);
        break;

    case FRV_AMOADDW:
        lift_amo_add(bb, instr, mapping, ip, Type::i32);
        break;
    case FRV_AMOADDD:
        lift_amo_add(bb, instr, mapping, ip, Type::i64);
        break;

    case FRV_AMOSWAPW:
        lift_amo_swap(bb, instr, mapping, ip, Type::i32);
        break;
    case FRV_AMOSWAPD:
        lift_amo_swap(bb, instr, mapping, ip, Type::i64);
        break;

    case FRV_AMOXORW:
        lift_amo_xor(bb, instr, mapping, ip, Type::i32);
        break;
    case FRV_AMOXORD:
        lift_amo_xor(bb, instr, mapping, ip, Type::i64);
        break;

    case FRV_AMOORW:
        lift_amo_or(bb, instr, mapping, ip, Type::i32);
        break;
    case FRV_AMOORD:
        lift_amo_or(bb, instr, mapping, ip, Type::i64);
        break;

    case FRV_AMOANDW:
        lift_amo_and(bb, instr, mapping, ip, Type::i32);
        break;
    case FRV_AMOANDD:
        lift_amo_and(bb, instr, mapping, ip, Type::i64);
        break;

    case FRV_AMOMINW:
        lift_amo_min(bb, instr, mapping, ip, Type::i32, true);
        break;
    case FRV_AMOMIND:
        lift_amo_min(bb, instr, mapping, ip, Type::i64, true);
        break;
    case FRV_AMOMINUW:
        lift_amo_min(bb, instr, mapping, ip, Type::i32, false);
        break;
    case FRV_AMOMINUD:
        lift_amo_min(bb, instr, mapping, ip, Type::i64, false);
        break;

    case FRV_AMOMAXW:
        lift_amo_max(bb, instr, mapping, ip, Type::i32, true);
        break;
    case FRV_AMOMAXD:
        lift_amo_max(bb, instr, mapping, ip, Type::i64, true);
        break;
    case FRV_AMOMAXUW:
        lift_amo_max(bb, instr, mapping, ip, Type::i32, false);
        break;
    case FRV_AMOMAXUD:
        lift_amo_max(bb, instr, mapping, ip, Type::i64, false);
        break;
#if 0
    /* ziscr */
    case FRV_CSRRW:
        lift_csr_read_write(bb, instr, mapping, ip, false);
        break;
    case FRV_CSRRS:
        lift_csr_read_set(bb, instr, mapping, ip, false);
        break;
    case FRV_CSRRC:
        lift_csr_read_clear(bb, instr, mapping, ip, false);
        break;
    case FRV_CSRRWI:
        lift_csr_read_write(bb, instr, mapping, ip, true);
        break;
    case FRV_CSRRSI:
        lift_csr_read_set(bb, instr, mapping, ip, true);
        break;
    case FRV_CSRRCI:
        lift_csr_read_clear(bb, instr, mapping, ip, true);
        break;

    /* F Extension */
    case FRV_FLW:
        lift_load(bb, instr, mapping, ip, Type::f32, false);
        break;
    case FRV_FSW:
        lift_store(bb, instr, mapping, ip, Type::f32);
        break;
    case FRV_FMVXW:
        lift_float_move(bb, instr, mapping, ip, Type::f32, Type::i32);
        break;
    case FRV_FMVWX:
        lift_float_move(bb, instr, mapping, ip, Type::i32, Type::f32);
        break;
    case FRV_FCLASSS:
        lift_fclass(bb, instr, mapping, ip, Type::f32);
        break;
    case FRV_FMADDS:
        lift_float_fma(bb, instr, mapping, ip, Instruction::fmadd, Type::f32);
        break;
    case FRV_FMSUBS:
        lift_float_fma(bb, instr, mapping, ip, Instruction::fmsub, Type::f32);
        break;
    case FRV_FNMSUBS:
        lift_float_fma(bb, instr, mapping, ip, Instruction::fnmadd, Type::f32);
        break;
    case FRV_FNMADDS:
        lift_float_fma(bb, instr, mapping, ip, Instruction::fnmsub, Type::f32);
        break;
    case FRV_FADDS:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::add, Type::f32);
        break;
    case FRV_FSUBS:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::sub, Type::f32);
        break;
    case FRV_FMULS:
        lift_mul(bb, instr, mapping, ip, Instruction::fmul, Type::f32);
        break;
    case FRV_FDIVS:
        lift_float_div(bb, instr, mapping, ip, Type::f32);
        break;
    case FRV_FSQRTS:
        lift_float_sqrt(bb, instr, mapping, ip, Type::f32);
        break;
    case FRV_FSGNJS:
    case FRV_FSGNJNS:
    case FRV_FSGNJXS:
        lift_float_sign_injection(bb, instr, mapping, ip, Type::f32);
        break;
    case FRV_FMINS:
        lift_float_min_max(bb, instr, mapping, ip, Instruction::min, Type::f32);
        break;
    case FRV_FMAXS:
        lift_float_min_max(bb, instr, mapping, ip, Instruction::max, Type::f32);
        break;
    case FRV_FLES:
        lift_float_comparison(bb, instr, mapping, ip, Instruction::sle, Type::f32);
        break;
    case FRV_FLTS:
        lift_float_comparison(bb, instr, mapping, ip, Instruction::slt, Type::f32);
        break;
    case FRV_FEQS:
        lift_float_comparison(bb, instr, mapping, ip, Instruction::seq, Type::f32);
        break;
    case FRV_FCVTWS:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f32, Type::i32, true);
        break;
    case FRV_FCVTWUS:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f32, Type::i32, false);
        break;
    case FRV_FCVTLS:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f32, Type::i64, true);
        break;
    case FRV_FCVTLUS:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f32, Type::i64, false);
        break;
    case FRV_FCVTSW:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::i32, Type::f32, true);
        break;
    case FRV_FCVTSWU:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::i32, Type::f32, false);
        break;
    case FRV_FCVTSL:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::i64, Type::f32, true);
        break;
    case FRV_FCVTSLU:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::i64, Type::f32, false);
        break;

        /* D Extension */

    case FRV_FLD:
        lift_load(bb, instr, mapping, ip, Type::f64, false);
        break;
    case FRV_FSD:
        lift_store(bb, instr, mapping, ip, Type::f64);
        break;
    case FRV_FMVXD:
        lift_float_move(bb, instr, mapping, ip, Type::f64, Type::i64);
        break;
    case FRV_FMVDX:
        lift_float_move(bb, instr, mapping, ip, Type::i64, Type::f64);
        break;
    case FRV_FCLASSD:
        lift_fclass(bb, instr, mapping, ip, Type::f64);
        break;
    case FRV_FMADDD:
        lift_float_fma(bb, instr, mapping, ip, Instruction::fmadd, Type::f64);
        break;
    case FRV_FMSUBD:
        lift_float_fma(bb, instr, mapping, ip, Instruction::fmsub, Type::f64);
        break;
    case FRV_FNMSUBD:
        lift_float_fma(bb, instr, mapping, ip, Instruction::fnmadd, Type::f64);
        break;
    case FRV_FNMADDD:
        lift_float_fma(bb, instr, mapping, ip, Instruction::fnmsub, Type::f64);
        break;
    case FRV_FADDD:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::add, Type::f64);
        break;
    case FRV_FSUBD:
        lift_arithmetical_logical(bb, instr, mapping, ip, Instruction::sub, Type::f64);
        break;
    case FRV_FMULD:
        lift_mul(bb, instr, mapping, ip, Instruction::fmul, Type::f64);
        break;
    case FRV_FDIVD:
        lift_float_div(bb, instr, mapping, ip, Type::f64);
        break;
    case FRV_FSQRTD:
        lift_float_sqrt(bb, instr, mapping, ip, Type::f64);
        break;
    case FRV_FSGNJD:
    case FRV_FSGNJND:
    case FRV_FSGNJXD:
        lift_float_sign_injection(bb, instr, mapping, ip, Type::f64);
        break;
    case FRV_FMIND:
        lift_float_min_max(bb, instr, mapping, ip, Instruction::min, Type::f64);
        break;
    case FRV_FMAXD:
        lift_float_min_max(bb, instr, mapping, ip, Instruction::max, Type::f64);
        break;
    case FRV_FLED:
        lift_float_comparison(bb, instr, mapping, ip, Instruction::sle, Type::f64);
        break;
    case FRV_FLTD:
        lift_float_comparison(bb, instr, mapping, ip, Instruction::slt, Type::f64);
        break;
    case FRV_FEQD:
        lift_float_comparison(bb, instr, mapping, ip, Instruction::seq, Type::f64);
        break;
    case FRV_FCVTSD:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f64, Type::f32, true);
        break;
    case FRV_FCVTDS:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f32, Type::f64, true);
        break;
    case FRV_FCVTWD:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f64, Type::i32, true);
        break;
    case FRV_FCVTWUD:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f64, Type::i32, false);
        break;
    case FRV_FCVTLD:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f64, Type::i64, true);
        break;
    case FRV_FCVTLUD:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::f64, Type::i64, false);
        break;
    case FRV_FCVTDW:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::i32, Type::f64, true);
        break;
    case FRV_FCVTDWU:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::i32, Type::f64, false);
        break;
    case FRV_FCVTDL:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::i64, Type::f64, true);
        break;
    case FRV_FCVTDLU:
        lift_float_integer_conversion(bb, instr, mapping, ip, Type::i64, Type::f64, false);
        break;
#endif

    default:
        char instr_str[16];
        frv_format(&instr.instr, 16, instr_str);

        std::stringstream str;
        str << "Encountered invalid instruction during lifting: " << instr_str;
        DEBUG_LOG(str.str());

        // TODO: add unreachable instruction
    }
}

inline void Lifter::lift_invalid([[maybe_unused]] BasicBlock *bb, [[maybe_unused]] uint64_t ip) {
    if (ENABLE_DEBUG) {
        std::stringstream str;
        str << "Encountered invalid instruction during lifting. (BasicBlock #0x" << std::hex << bb->id << ", address <0x" << ip << ">)";
        DEBUG_LOG(str.str());
    }
}

void Lifter::lift_slt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool is_unsigned, bool with_immediate) {
    // get operands for operations (the operands which were compared)
    SSAVar *first_operand = get_from_mapping(bb, mapping, instr.instr.rs1, ip);
    SSAVar *second_operand;

    if (with_immediate) {
        second_operand = load_immediate(bb, instr.instr.imm, ip, false);
    } else {
        second_operand = get_from_mapping(bb, mapping, instr.instr.rs2, ip);
    }

    // create variables for result
    SSAVar *one = load_immediate(bb, 1, ip, false);
    SSAVar *zero = load_immediate(bb, 0, ip, false);

    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(Type::i64, ip, instr.instr.rd);

    // create slt operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(is_unsigned ? Instruction::sltu : Instruction::slt);

    // set in- and outputs
    operation->set_inputs(first_operand, second_operand, one, zero);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    // write SSAVar of the result of the operation back to mapping
    write_to_mapping(mapping, destination, instr.instr.rd);
}

void Lifter::lift_auipc(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip) {
    // 1. load the immediate
    SSAVar *immediate = load_immediate(bb, instr.instr.imm, ip, false);

    // 2. load instruction pointer as immediate
    SSAVar *ip_immediate = load_immediate(bb, (int64_t)(ip), ip, true);

    // 3. add immediate to instruction pointer
    SSAVar *result = bb->add_var(Type::i64, ip, instr.instr.rd);
    {
        auto add_op = std::make_unique<Operation>(Instruction::add);
        add_op->set_inputs(ip_immediate, immediate);
        add_op->set_outputs(result);
        result->set_op(std::move(add_op));
    }
    // write SSAVar back to mapping
    write_to_mapping(mapping, result, instr.instr.rd);
}

void Lifter::lift_lui(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip) {
    // create the immediate loading operation (with built-in sign extension)
    SSAVar *immediate = load_immediate(bb, (int64_t)instr.instr.imm, ip, false);

    // write SSAVar back to mapping
    write_to_mapping(mapping, immediate, instr.instr.rd);
}

void Lifter::lift_fence([[maybe_unused]] BasicBlock *bb, [[maybe_unused]] const RV64Inst &instr, [[maybe_unused]] uint64_t ip) {
    if (ENABLE_DEBUG) {
        std::stringstream str;
        str << "Skipping " << str_decode_instr(&instr.instr) << " instruction. (BasicBlock #0x" << std::hex << bb->id << ", address <0x" << ip << ">)";
        DEBUG_LOG(str.str());
    }
}
