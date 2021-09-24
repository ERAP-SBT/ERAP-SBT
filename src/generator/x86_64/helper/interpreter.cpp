#include "frvdec.h"
#include "generator/syscall_ids.h"
#include "generator/x86_64/helper/helper.h"
#include "generator/x86_64/helper/rv64_syscalls.h"

#include <cstddef>
#include <cstdint>

namespace helper::interpreter {

constexpr inline size_t START_FP_STATICS = 33;

constexpr inline size_t FCSR_IDX = 65;

typedef union converter {
    uint32_t d32;
    int32_t i32;
    uint64_t d64;
    int64_t i64;
    float f32;
    double f64;
} converter;

#define AMO_OP(ptr_type, type) \
    { \
        ptr_type *ptr = reinterpret_cast<ptr_type *>(register_file[instr.rs1]); \
        type rs2_val = static_cast<type>(register_file[instr.rs2]); \
        if (instr.rd != 0) { \
            register_file[instr.rd] = sign_extend_int64_t(*ptr); \
        } \
        *ptr = operation(static_cast<type>(register_file[instr.rd]), rs2_val); \
        break; \
    }

#define CSR_OP(source, operation) \
    { \
        size_t csr_idx = evaluate_csr_index(instr.imm); \
        uint64_t src = source; \
        if (instr.rd != 0) { \
            register_file[instr.rd] = static_cast<uint64_t>(register_file[csr_idx]); \
        } \
        register_file[csr_idx] operation static_cast<uint32_t>(src); \
        break; \
    }

#define FP_THREE_OP_FLOAT() \
    { \
        converter conv1; \
        converter conv2; \
        converter conv3; \
        conv1.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]); \
        conv2.d32 = static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]); \
        conv3.d32 = static_cast<uint32_t>(register_file[instr.rs3 + START_FP_STATICS]); \
        conv1.f32 = operation(conv1.f32, conv2.f32, conv3.f32); \
        register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(conv1.d32); \
        break; \
    }

#define FP_THREE_OP_DOUBLE() \
    { \
        converter conv1; \
        converter conv2; \
        converter conv3; \
        conv1.d64 = register_file[instr.rs1 + START_FP_STATICS]; \
        conv2.d64 = register_file[instr.rs2 + START_FP_STATICS]; \
        conv3.d64 = register_file[instr.rs3 + START_FP_STATICS]; \
        conv1.f64 = operation(conv1.f64, conv2.f64, conv3.f64); \
        register_file[instr.rd + START_FP_STATICS] = conv1.d64; \
        break; \
    }

#define FP_TWO_OP_FLOAT(fp_dest_reg) \
    if (fp_dest_reg || instr.rd != 0) { \
        converter conv1; \
        converter conv2; \
        converter conv3; \
        conv1.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]); \
        conv2.d32 = static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]); \
        conv3.f32 = operation(conv1.f32, conv2.f32); \
        register_file[instr.rd + (fp_dest_reg ? START_FP_STATICS : 0)] = static_cast<uint64_t>(conv3.d32); \
    }

#define FP_TWO_OP_DOUBLE(fp_dest_reg) \
    if (fp_dest_reg || instr.rd != 0) { \
        converter conv1; \
        converter conv2; \
        converter conv3; \
        conv1.d64 = register_file[instr.rs1 + START_FP_STATICS]; \
        conv2.d64 = register_file[instr.rs2 + START_FP_STATICS]; \
        conv3.f64 = operation(conv1.f64, conv2.f64); \
        register_file[instr.rd + (fp_dest_reg ? START_FP_STATICS : 0)] = conv3.d64; \
    }

/* from compiled code */
extern "C" uint64_t register_file[];
extern "C" uint64_t ijump_lookup_base;
extern "C" uint64_t ijump_lookup[];

void trace(uint64_t addr, const FrvInst *instr) {
    puts("TRACE: 0x");

    print_hex64(addr);

    puts(" : ");

    char buf[32];
    frv_format(instr, sizeof(buf), buf);
    puts(buf);
    puts("\n");
}

// TODO: bounds check
uint64_t ijump_lookup_for_addr(uint64_t addr) { return ijump_lookup[(addr - ijump_lookup_base) / 0x2]; }

/* make the code a bit clearer */
constexpr uint64_t sign_extend_int64_t(int32_t v) { return static_cast<int64_t>(v); }

size_t evaluate_csr_index(uint32_t csr_id) {
    switch (csr_id) {
    case 1:
    case 2:
    case 3:
        return FCSR_IDX;
    default:
        panic("Not known csr register!");
        break;
    }
    panic("Not known csr register!");
    return -1;
}

void trace_dump_state(uint64_t pc) {
    puts("TRACE: STATE");

    puts("\npc:  ");
    print_hex64(pc);
    puts("\ns00: ");
    print_hex64(register_file[0]);
    puts("\ns01: ");
    print_hex64(register_file[1]);
    puts("\ns02: ");
    print_hex64(register_file[2]);
    puts("\ns03: ");
    print_hex64(register_file[3]);
    puts("\ns04: ");
    print_hex64(register_file[4]);
    puts("\ns05: ");
    print_hex64(register_file[5]);
    puts("\ns06: ");
    print_hex64(register_file[6]);
    puts("\ns07: ");
    print_hex64(register_file[7]);
    puts("\ns08: ");
    print_hex64(register_file[8]);
    puts("\ns09: ");
    print_hex64(register_file[9]);
    puts("\ns10: ");
    print_hex64(register_file[10]);
    puts("\ns11: ");
    print_hex64(register_file[11]);
    puts("\ns12: ");
    print_hex64(register_file[12]);
    puts("\ns13: ");
    print_hex64(register_file[13]);
    puts("\ns14: ");
    print_hex64(register_file[14]);
    puts("\ns15: ");
    print_hex64(register_file[15]);
    puts("\ns16: ");
    print_hex64(register_file[16]);
    puts("\ns17: ");
    print_hex64(register_file[17]);
    puts("\ns18: ");
    print_hex64(register_file[18]);
    puts("\ns19: ");
    print_hex64(register_file[19]);
    puts("\ns20: ");
    print_hex64(register_file[20]);
    puts("\ns21: ");
    print_hex64(register_file[21]);
    puts("\ns22: ");
    print_hex64(register_file[22]);
    puts("\ns23: ");
    print_hex64(register_file[23]);
    puts("\ns24: ");
    print_hex64(register_file[24]);
    puts("\ns25: ");
    print_hex64(register_file[25]);
    puts("\ns26: ");
    print_hex64(register_file[26]);
    puts("\ns27: ");
    print_hex64(register_file[27]);
    puts("\ns28: ");
    print_hex64(register_file[28]);
    puts("\ns29: ");
    print_hex64(register_file[29]);
    puts("\ns30: ");
    print_hex64(register_file[30]);
    puts("\ns31: ");
    print_hex64(register_file[31]);

    puts("\n");
}

/**
 * @param target unresolved jump target address
 */
extern "C" uint64_t unresolved_ijump_handler(uint64_t target) {
    // puts("TRACE: handler, target: ");
    // print_hex64(target);
    // puts("\n");

    uint64_t pc = target;

    do {
        bool jump = false;
        FrvInst instr;
        const int r = frv_decode(0x1000, reinterpret_cast<const uint8_t *>(pc), FRV_RV64, &instr);

        if (r == FRV_UNDEF) {
            trace_dump_state(pc);
            panic("Unable to decode instruction");
        } else if (r == FRV_PARTIAL) {
            trace_dump_state(pc);
            panic("partial instruction");
        } else if (r < 0) {
            trace_dump_state(pc);
            panic("undefined");
        }

        // TODO: we might be able to ignore everything with rd=0 as either HINT or NOP instructions
        switch (instr.mnem) {
        /* 2.4 Integer Computational Instructions */
        case FRV_ADDI:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] + sign_extend_int64_t(instr.imm);
            }
            break;
        case FRV_ADDIW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(register_file[instr.rs1]) + instr.imm);
            }
            break;
        case FRV_ADD:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] + register_file[instr.rs2];
            }
            break;
        case FRV_ADDW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(register_file[instr.rs1]) + static_cast<int32_t>(register_file[instr.rs2]));
            }
            break;
        case FRV_SUB:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] - register_file[instr.rs2];
            }
            break;
        case FRV_SUBW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(register_file[instr.rs1]) - static_cast<int32_t>(register_file[instr.rs2]));
            }
            break;

        case FRV_SLTI:
            if (instr.rd != 0) {
                register_file[instr.rd] = static_cast<int64_t>(register_file[instr.rs1]) < static_cast<int64_t>(instr.imm);
            }
            break;
        case FRV_SLTIU:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] < static_cast<uint64_t>(instr.imm);
            }
            break;
        case FRV_SLT:
            if (instr.rd != 0) {
                register_file[instr.rd] = static_cast<int64_t>(register_file[instr.rs1]) < static_cast<int64_t>(register_file[instr.rs1]);
            }
            break;
        case FRV_SLTU:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] < register_file[instr.rs2];
            }
            break;

        case FRV_ANDI:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] & sign_extend_int64_t(instr.imm);
            }
            break;
        case FRV_ORI:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] | sign_extend_int64_t(instr.imm);
            }
            break;
        case FRV_XORI:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] ^ sign_extend_int64_t(instr.imm);
            }
            break;
        case FRV_AND:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] & register_file[instr.rs2];
            }
            break;
        case FRV_OR:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] | register_file[instr.rs2];
            }
            break;
        case FRV_XOR:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] ^ register_file[instr.rs2];
            }
            break;

        case FRV_SLLI:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] << instr.imm;
            }
            break;
        case FRV_SRLI:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] >> instr.imm;
            }
            break;
        case FRV_SRAI:
            if (instr.rd != 0) {
                register_file[instr.rd] = static_cast<int64_t>(register_file[instr.rs1]) >> instr.imm;
            }
            break;
        case FRV_SLLIW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<uint32_t>(register_file[instr.rs1]) << instr.imm);
            }
            break;
        case FRV_SRLIW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<uint32_t>(register_file[instr.rs1]) >> instr.imm);
            }
            break;
        case FRV_SRAIW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(register_file[instr.rs1]) >> instr.imm);
            }
            break;
        case FRV_SLL:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] << (register_file[instr.rs2] & 0x3F);
            }
            break;
        case FRV_SRL:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] >> (register_file[instr.rs2] & 0x3F);
            }
            break;
        case FRV_SRA:
            if (instr.rd != 0) {
                register_file[instr.rd] = static_cast<int64_t>(register_file[instr.rs1]) >> (register_file[instr.rs2] & 0x3F);
            }
            break;
        case FRV_SLLW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<uint32_t>(register_file[instr.rs1]) << (register_file[instr.rs2] & 0x1F));
            }
            break;
        case FRV_SRLW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<uint32_t>(register_file[instr.rs1]) >> (register_file[instr.rs2] & 0x1F));
            }
            break;
        case FRV_SRAW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(register_file[instr.rs1]) >> (register_file[instr.rs2] & 0x1F));
            }
            break;

        case FRV_LUI:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(instr.imm);
            }
            break;
        case FRV_AUIPC:
            if (instr.rd != 0) {
                register_file[instr.rd] = pc + instr.imm;
            }
            break;

            /* 2.5 Control Transfer Instructions */

        case FRV_JAL:
            if (instr.rd != 0) {
                register_file[instr.rd] = pc + r;
            }
            pc += static_cast<int64_t>(instr.imm);
            jump = true;
            break;
        case FRV_JALR: {
            uint64_t jmp_addr = (register_file[instr.rs1] + static_cast<int64_t>(instr.imm)) & 0xFFFF'FFFF'FFFF'FFFE;
            if (instr.rd != 0) {
                register_file[instr.rd] = pc + r;
            }
            pc = jmp_addr;
            jump = true;
            break;
        }
        case FRV_BEQ:
            if (register_file[instr.rs1] == register_file[instr.rs2]) {
                pc += static_cast<int64_t>(instr.imm);
                jump = true;
            }
            break;
        case FRV_BNE:
            if (register_file[instr.rs1] != register_file[instr.rs2]) {
                pc += static_cast<int64_t>(instr.imm);
                jump = true;
            }
            break;
        case FRV_BLT:
            if (static_cast<int64_t>(register_file[instr.rs1]) < static_cast<int64_t>(register_file[instr.rs2])) {
                pc += static_cast<int64_t>(instr.imm);
                jump = true;
            }
            break;
        case FRV_BLTU:
            if (register_file[instr.rs1] < register_file[instr.rs2]) {
                pc += static_cast<int64_t>(instr.imm);
                jump = true;
            }
            break;
        case FRV_BGE:
            if (static_cast<int64_t>(register_file[instr.rs1]) >= static_cast<int64_t>(register_file[instr.rs2])) {
                pc += static_cast<int64_t>(instr.imm);
                jump = true;
            }
            break;
        case FRV_BGEU:
            if (register_file[instr.rs1] >= register_file[instr.rs2]) {
                pc += static_cast<int64_t>(instr.imm);
                jump = true;
            }
            break;

        /* 2.6 Load and Store Instructions */
        case FRV_SD: {
            uint64_t *ptr = reinterpret_cast<uint64_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
            *ptr = register_file[instr.rs2];
            break;
        }
        case FRV_SW: {
            uint32_t *ptr = reinterpret_cast<uint32_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
            *ptr = static_cast<uint32_t>(register_file[instr.rs2]);
            break;
        }
        case FRV_SH: {
            uint16_t *ptr = reinterpret_cast<uint16_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
            *ptr = static_cast<uint16_t>(register_file[instr.rs2]);
            break;
        }
        case FRV_SB: {
            uint8_t *ptr = reinterpret_cast<uint8_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
            *ptr = static_cast<uint8_t>(register_file[instr.rs2]);
            break;
        }

        case FRV_LD:
            if (instr.rd != 0) {
                uint64_t *ptr = reinterpret_cast<uint64_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
                register_file[instr.rd] = *ptr;
            }
            break;
        case FRV_LW:
            if (instr.rd != 0) {
                int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
                register_file[instr.rd] = static_cast<int64_t>(*ptr);
            }
            break;
        case FRV_LWU:
            if (instr.rd != 0) {
                uint32_t *ptr = reinterpret_cast<uint32_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
                register_file[instr.rd] = static_cast<uint64_t>(*ptr);
            }
            break;
        case FRV_LH:
            if (instr.rd != 0) {
                int16_t *ptr = reinterpret_cast<int16_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
                register_file[instr.rd] = static_cast<int64_t>(*ptr);
            }
            break;
        case FRV_LHU:
            if (instr.rd != 0) {
                uint16_t *ptr = reinterpret_cast<uint16_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
                register_file[instr.rd] = static_cast<uint64_t>(*ptr);
            }
            break;
        case FRV_LB:
            if (instr.rd != 0) {
                int8_t *ptr = reinterpret_cast<int8_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
                register_file[instr.rd] = static_cast<int64_t>(*ptr);
            }
            break;
        case FRV_LBU:
            if (instr.rd != 0) {
                uint8_t *ptr = reinterpret_cast<uint8_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
                register_file[instr.rd] = static_cast<uint64_t>(*ptr);
            }
            break;

        /* 2.7 Memory Ordering Instructions */
        case FRV_FENCE:
            [[fallthrough]];
        case FRV_FENCEI:
            // ignore
            break;

        /* 2.8 Environment Call and Breakpoints */
        case FRV_ECALL:
            register_file[10] = syscall_impl(register_file[17], register_file[10], register_file[11], register_file[12], register_file[13], register_file[14], register_file[15]);
            break;

        /* M extension */
        case FRV_MUL:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] * register_file[instr.rs2];
            }
            break;
        case FRV_MULW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(register_file[instr.rs1]) * static_cast<int32_t>(register_file[instr.rs2]));
            }
            break;
        case FRV_MULH:
            if (instr.rd != 0) {
                register_file[instr.rd] = (static_cast<__int128_t>(static_cast<int64_t>(register_file[instr.rs1])) * static_cast<__int128_t>(static_cast<int64_t>(register_file[instr.rs2]))) >> 64;
            }
            break;
        case FRV_MULHU:
            if (instr.rd != 0) {
                register_file[instr.rd] = (static_cast<__uint128_t>(register_file[instr.rs1]) * static_cast<__uint128_t>(register_file[instr.rs2])) >> 64;
            }
            break;
        case FRV_MULHSU:
            if (instr.rd != 0) {
                register_file[instr.rd] = (static_cast<__int128_t>(static_cast<int64_t>(register_file[instr.rs1])) * static_cast<__uint128_t>(register_file[instr.rs2])) >> 64;
            }
            break;

        case FRV_DIV:
            if (instr.rd != 0) {
                register_file[instr.rd] = static_cast<int64_t>(register_file[instr.rs1]) / static_cast<int64_t>(register_file[instr.rs2]);
            }
            break;
        case FRV_DIVU:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] / register_file[instr.rs2];
            }
            break;
        case FRV_DIVW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(register_file[instr.rs1]) / static_cast<int32_t>(register_file[instr.rs2]));
            }
            break;
        case FRV_DIVUW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<uint32_t>(register_file[instr.rs1]) / static_cast<uint32_t>(register_file[instr.rs2]));
            }
            break;
        case FRV_REM:
            if (instr.rd != 0) {
                register_file[instr.rd] = static_cast<int64_t>(register_file[instr.rs1]) % static_cast<int64_t>(register_file[instr.rs2]);
            }
            break;
        case FRV_REMU:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1] % register_file[instr.rs2];
            }
            break;
        case FRV_REMW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(register_file[instr.rs1]) % static_cast<int32_t>(register_file[instr.rs2]));
            }
            break;
        case FRV_REMUW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<uint32_t>(register_file[instr.rs1]) % static_cast<uint32_t>(register_file[instr.rs2]));
            }
            break;

        /* A extension */
        case FRV_LRW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*reinterpret_cast<int32_t *>(register_file[instr.rs1]));
            }
            break;
        case FRV_LRD:
            if (instr.rd != 0) {
                register_file[instr.rd] = *reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            }
            break;
        case FRV_SCW:
            *reinterpret_cast<uint32_t *>(register_file[instr.rs1]) = static_cast<uint32_t>(register_file[instr.rs2]);
            if (instr.rd != 0) {
                register_file[instr.rd] = 0;
            }
            break;
        case FRV_SCD:
            *reinterpret_cast<uint64_t *>(register_file[instr.rs1]) = register_file[instr.rs2];
            if (instr.rd != 0) {
                register_file[instr.rd] = 0;
            }
            break;
        case FRV_AMOSWAPW:
#define operation(val1, val2) val2
            AMO_OP(int32_t, uint32_t);
#undef operation
        case FRV_AMOSWAPD:
#define operation(val1, val2) val2
            AMO_OP(int64_t, uint64_t);
#undef operation
        case FRV_AMOADDW:
#define operation(val1, val2) val1 + val2
            AMO_OP(int32_t, uint32_t);
#undef operation
        case FRV_AMOADDD:
#define operation(val1, val2) val1 + val2
            AMO_OP(int64_t, uint64_t);
#undef operation
        case FRV_AMOANDW:
#define operation(val1, val2) val1 &val2
            AMO_OP(int32_t, uint32_t);
#undef operation
        case FRV_AMOANDD:
#define operation(val1, val2) val1 &val2
            AMO_OP(int64_t, uint64_t);
#undef operation
        case FRV_AMOORW:
#define operation(val1, val2) val1 | val2
            AMO_OP(int32_t, uint32_t);
#undef operation
        case FRV_AMOORD:
#define operation(val1, val2) val1 | val2
            AMO_OP(int64_t, uint64_t);
#undef operation
        case FRV_AMOXORW:
#define operation(val1, val2) val1 ^ val2
            AMO_OP(int32_t, uint32_t);
#undef operation
        case FRV_AMOXORD:
#define operation(val1, val2) val1 ^ val2
            AMO_OP(int64_t, uint64_t);
#undef operation
        case FRV_AMOMAXW:
#define operation(val1, val2) (val1 > val2) ? val1 : val2
            AMO_OP(int32_t, int32_t);
#undef operation
        case FRV_AMOMAXD:
#define operation(val1, val2) (val1 > val2) ? val1 : val2
            AMO_OP(int64_t, int64_t);
#undef operation
        case FRV_AMOMAXUW:
#define operation(val1, val2) (val1 > val2) ? val1 : val2
            AMO_OP(int32_t, uint32_t);
#undef operation
        case FRV_AMOMAXUD:
#define operation(val1, val2) (val1 > val2) ? val1 : val2
            AMO_OP(int64_t, uint64_t);
#undef operation
        case FRV_AMOMINW:
#define operation(val1, val2) (val1 < val2) ? val1 : val2
            AMO_OP(int32_t, int32_t);
#undef operation
        case FRV_AMOMIND:
#define operation(val1, val2) (val1 < val2) ? val1 : val2
            AMO_OP(int64_t, int64_t);
#undef operation
        case FRV_AMOMINUW:
#define operation(val1, val2) (val1 < val2) ? val1 : val2
            AMO_OP(int32_t, uint32_t);
#undef operation
        case FRV_AMOMINUD:
#define operation(val1, val2) (val1 < val2) ? val1 : val2
            AMO_OP(int64_t, uint64_t);
#undef operation

        /* Ziscr extension */
        case FRV_CSRRW:
            CSR_OP(register_file[instr.rs1], =);
        case FRV_CSRRS:
            CSR_OP(register_file[instr.rs1], |=);
        case FRV_CSRRC:
            CSR_OP(register_file[instr.rs1], &= ~);
        case FRV_CSRRWI:
            CSR_OP(instr.rs1, =);
        case FRV_CSRRSI:
            CSR_OP(instr.rs1, |=);
        case FRV_CSRRCI:
            CSR_OP(instr.rs1, &= ~);

        /* F extension */
        case FRV_FLW: {
            uint32_t *ptr = reinterpret_cast<uint32_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*ptr);
            break;
        }
        case FRV_FLD:
            register_file[instr.rd + START_FP_STATICS] = *reinterpret_cast<uint64_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
            break;
        case FRV_FSW: {
            uint32_t *ptr = reinterpret_cast<uint32_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
            *ptr = static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]);
            break;
        }
        case FRV_FSD: {
            uint64_t *ptr = reinterpret_cast<uint64_t *>(register_file[instr.rs1] + sign_extend_int64_t(instr.imm));
            *ptr = register_file[instr.rs2 + START_FP_STATICS];
            break;
        }

        case FRV_FADDS:
#define operation(val1, val2) val1 + val2
            FP_TWO_OP_FLOAT(true);
#undef operation
            break;
        case FRV_FADDD:
#define operation(val1, val2) val1 + val2
            FP_TWO_OP_DOUBLE(true);
#undef operation
            break;
        case FRV_FSUBS:
#define operation(val1, val2) val1 - val2
            FP_TWO_OP_FLOAT(true);
#undef operation
            break;
        case FRV_FSUBD:
#define operation(val1, val2) val1 - val2
            FP_TWO_OP_DOUBLE(true);
#undef operation
            break;
        case FRV_FMULS:
#define operation(val1, val2) val1 *val2
            FP_TWO_OP_FLOAT(true);
#undef operation
            break;
        case FRV_FMULD:
#define operation(val1, val2) val1 *val2
            FP_TWO_OP_DOUBLE(true);
#undef operation
            break;
        case FRV_FDIVS:
#define operation(val1, val2) val1 / val2
            FP_TWO_OP_FLOAT(true);
#undef operation
            break;
        case FRV_FDIVD:
#define operation(val1, val2) val1 / val2
            FP_TWO_OP_DOUBLE(true);
#undef operation
            break;
        case FRV_FMINS:
#define operation(val1, val2) (val1 < val2) ? val1 : val2
            FP_TWO_OP_FLOAT(true);
#undef operation
            break;
        case FRV_FMIND:
#define operation(val1, val2) (val1 < val2) ? val1 : val2
            FP_TWO_OP_DOUBLE(true);
#undef operation
            break;
        case FRV_FMAXS:
#define operation(val1, val2) (val1 > val2) ? val1 : val2
            FP_TWO_OP_FLOAT(true);
#undef operation
            break;
        case FRV_FMAXD:
#define operation(val1, val2) (val1 > val2) ? val1 : val2
            FP_TWO_OP_DOUBLE(true);
#undef operation
            break;
        case FRV_FSQRTS: {
            float *src_ptr = reinterpret_cast<float *>(&register_file[instr.rs1 + START_FP_STATICS]);
            float res;
            __asm__ __volatile__("sqrtss %1, %0" : "=x"(res) : "x"(*src_ptr));
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }
        case FRV_FSQRTD: {
            double *src_ptr = reinterpret_cast<double *>(&register_file[instr.rs1 + START_FP_STATICS]);
            double res;
            __asm__ __volatile__("sqrtss %1, %0" : "=x"(res) : "x"(*src_ptr));
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }

        case FRV_FMADDS:
#define operation(val1, val2, val3) val1 *val2 + val3
            FP_THREE_OP_FLOAT();
#undef operation
        case FRV_FMSUBS:
#define operation(val1, val2, val3) val1 *val2 - val3
            FP_THREE_OP_FLOAT();
#undef operation
        case FRV_FNMADDS:
#define operation(val1, val2, val3) -(val1 * val2 + val3)
            FP_THREE_OP_FLOAT();
#undef operation
        case FRV_FNMSUBS:
#define operation(val1, val2, val3) -val1 *val2 + val3
            FP_THREE_OP_FLOAT();
#undef operation

        case FRV_FMADDD:
#define operation(val1, val2, val3) val1 *val2 + val3
            FP_THREE_OP_DOUBLE();
#undef operation
        case FRV_FMSUBD:
#define operation(val1, val2, val3) val1 *val2 - val3
            FP_THREE_OP_DOUBLE();
#undef operation
        case FRV_FNMADDD:
#define operation(val1, val2, val3) -(val1 * val2 + val3)
            FP_THREE_OP_DOUBLE();
#undef operation
        case FRV_FNMSUBD:
#define operation(val1, val2, val3) -val1 *val2 + val3
            FP_THREE_OP_DOUBLE();
#undef operation

        case FRV_FCVTWS:
            if (instr.rd != 0) {
                float *src_ptr = reinterpret_cast<float *>(register_file[instr.rs1 + START_FP_STATICS]);
                register_file[instr.rd] = static_cast<int64_t>(static_cast<int32_t>(*src_ptr));
            }
            break;
        case FRV_FCVTWUS:
            if (instr.rd != 0) {
                float *src_ptr = reinterpret_cast<float *>(register_file[instr.rs1 + START_FP_STATICS]);
                register_file[instr.rd] = static_cast<uint64_t>(static_cast<uint32_t>(*src_ptr));
            }
            break;
        case FRV_FCVTLS:
            if (instr.rd != 0) {
                float *src_ptr = reinterpret_cast<float *>(register_file[instr.rs1 + START_FP_STATICS]);
                register_file[instr.rd] = static_cast<int64_t>(*src_ptr);
            }
            break;
        case FRV_FCVTLUS:
            if (instr.rd != 0) {
                float *src_ptr = reinterpret_cast<float *>(register_file[instr.rs1 + START_FP_STATICS]);
                register_file[instr.rd] = static_cast<uint64_t>(*src_ptr);
            }
            break;

        case FRV_FCVTSW: {
            float res = static_cast<float>(static_cast<int32_t>(register_file[instr.rs1]));
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }
        case FRV_FCVTSWU: {
            float res = static_cast<float>(static_cast<uint32_t>(register_file[instr.rs1]));
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }
        case FRV_FCVTSL: {
            float res = static_cast<float>(static_cast<int64_t>(register_file[instr.rs1]));
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }
        case FRV_FCVTSLU: {
            float res = static_cast<float>(register_file[instr.rs1]);
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }

        case FRV_FCVTWD:
            if (instr.rd != 0) {
                double *src_ptr = reinterpret_cast<double *>(register_file[instr.rs1 + START_FP_STATICS]);
                register_file[instr.rd] = static_cast<int64_t>(static_cast<int32_t>(*src_ptr));
            }
            break;
        case FRV_FCVTWUD:
            if (instr.rd != 0) {
                double *src_ptr = reinterpret_cast<double *>(register_file[instr.rs1 + START_FP_STATICS]);
                register_file[instr.rd] = static_cast<uint64_t>(static_cast<uint32_t>(*src_ptr));
            }
            break;
        case FRV_FCVTLD:
            if (instr.rd != 0) {
                double *src_ptr = reinterpret_cast<double *>(register_file[instr.rs1 + START_FP_STATICS]);
                register_file[instr.rd] = static_cast<int64_t>(*src_ptr);
            }
            break;
        case FRV_FCVTLUD:
            if (instr.rd != 0) {
                double *src_ptr = reinterpret_cast<double *>(register_file[instr.rs1 + START_FP_STATICS]);
                register_file[instr.rd] = static_cast<uint64_t>(*src_ptr);
            }
            break;

        case FRV_FCVTDW: {
            double res = static_cast<double>(static_cast<int32_t>(register_file[instr.rs1]));
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }
        case FRV_FCVTDWU: {
            double res = static_cast<double>(static_cast<uint32_t>(register_file[instr.rs1]));
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }
        case FRV_FCVTDL: {
            double res = static_cast<double>(static_cast<int64_t>(register_file[instr.rs1]));
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }
        case FRV_FCVTDLU: {
            double res = static_cast<double>(register_file[instr.rs1]);
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }
        case FRV_FCVTDS: {
            converter conv;
            conv.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
            conv.f64 = static_cast<double>(conv.f32);
            register_file[instr.rd + START_FP_STATICS] = conv.f64;
            break;
        }
        case FRV_FCVTSD: {
            converter conv;
            conv.d64 = register_file[instr.rs1 + START_FP_STATICS];
            conv.f32 = static_cast<double>(conv.f64);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(conv.d32);
            break;
        }

        case FRV_FMVXW:
            if (instr.rd != 0) {
                register_file[instr.rd] = static_cast<int64_t>(static_cast<int32_t>(register_file[instr.rs1 + START_FP_STATICS]));
            }
            break;
        case FRV_FMVWX:
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(static_cast<uint32_t>(register_file[instr.rs1]));
            break;
        case FRV_FMVXD:
            if (instr.rd != 0) {
                register_file[instr.rd] = register_file[instr.rs1 + START_FP_STATICS];
            }
            break;
        case FRV_FMVDX:
            register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1];
            break;

        case FRV_FSGNJS:
            if (instr.rs1 == instr.rs2) {
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1];
            } else {
                register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>((static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]) & 0x7FFF'FFFF) |
                                                                                   (static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]) & 0x8000'0000));
            }
            break;
        case FRV_FSGNJD:
            if (instr.rs1 == instr.rs2) {
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1];
            } else {
                register_file[instr.rd + START_FP_STATICS] =
                    (register_file[instr.rs1 + START_FP_STATICS] & 0x7FFF'FFFF'FFFF'FFFF) | (register_file[instr.rs2 + START_FP_STATICS] & 0x8000'0000'0000'0000);
            }
            break;
        case FRV_FSGNJNS:
            if (instr.rs1 == instr.rs2) {
                // negate the floating point value (change sign bit)
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1] ^ 0x8000'0000;
            } else {
                register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>((static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]) & 0x7FFF'FFFF) |
                                                                                   (~static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]) & 0x8000'0000));
            }
            break;
        case FRV_FSGNJND:
            if (instr.rs1 == instr.rs2) {
                // negate the floating point value (change sign bit)
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1] ^ 0x8000'0000'0000'0000;
            } else {
                register_file[instr.rd + START_FP_STATICS] =
                    (register_file[instr.rs1 + START_FP_STATICS] & 0x7FFF'FFFF'FFFF'FFFF) | (~register_file[instr.rs2 + START_FP_STATICS] & 0x8000'0000'0000'0000);
            }
            break;
        case FRV_FSGNJXS: {
            if (instr.rs1 == instr.rs2) {
                // calculate the absulate value (set sign bit to zero)
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1] & 0x7FFF'FFFF;
            } else {
                uint32_t new_sign =
                    (static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]) & 0x8000'0000) ^ (static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]) & 0x8000'0000);
                register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>((static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]) & 0x7FFF'FFFF) | new_sign);
            }
            break;
        }
        case FRV_FSGNJXD:
            if (instr.rs1 == instr.rs2) {
                // calculate the absulate value (set sign bit to zero)
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1] & 0x7FFF'FFFF'FFFF'FFFF;
            } else {
                uint64_t new_sign = (register_file[instr.rs1 + START_FP_STATICS] & 0x8000'0000'0000'0000) ^ (register_file[instr.rs2 + START_FP_STATICS] & 0x8000'0000'0000'0000);
                register_file[instr.rd + START_FP_STATICS] = (register_file[instr.rs1 + START_FP_STATICS] & 0x7FFF'FFFF'FFFF'FFFF) | new_sign;
            }
            break;

        case FRV_FLTS:
#define operation(val1, val2) val1 < val2 ? 1 : 0
            FP_TWO_OP_FLOAT(false);
#undef operation
            break;
        case FRV_FLTD:
#define operation(val1, val2) val1 < val2 ? 1 : 0
            FP_TWO_OP_DOUBLE(false);
#undef operation
            break;
        case FRV_FLES:
#define operation(val1, val2) val1 <= val2 ? 1 : 0
            FP_TWO_OP_FLOAT(false);
#undef operation
            break;
        case FRV_FLED:
#define operation(val1, val2) val1 <= val2 ? 1 : 0
            FP_TWO_OP_DOUBLE(false);
#undef operation
            break;
        case FRV_FEQS:
#define operation(val1, val2) val1 == val2 ? 1 : 0
            FP_TWO_OP_FLOAT(false);
#undef operation
            break;
        case FRV_FEQD:
#define operation(val1, val2) val1 == val2 ? 1 : 0
            FP_TWO_OP_DOUBLE(false);
#undef operation
            break;
        case FRV_FCLASSS:
        case FRV_FCLASSD:
            trace(pc, &instr);
            panic("FLCASS[S|D] is currently not implemented!");
            break;
        default:
            trace(pc, &instr);
            trace_dump_state(pc);
            panic("instruction not implemented\n");
            break;
        }

        if (!jump) {
            pc += r; // FIXME: is increment PC a pre or post operation ?
        }
    } while (ijump_lookup_for_addr(pc) == 0);

    // puts("TRACE: found compiled basic block, addr: ");
    // print_hex64(pc);
    // puts("\n");

    // trace_dump_state(pc);

    // puts("\n");

    /* At this point we have found a valid entry point back into
     * the compiled BasicBlocks
     */
    return ijump_lookup_for_addr(pc);
}

} // namespace helper::interpreter