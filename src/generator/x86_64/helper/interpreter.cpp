#include "frvdec.h"
#include "generator/syscall_ids.h"
#include "generator/x86_64/helper/helper.h"
#include "generator/x86_64/helper/rv64_syscalls.h"

#include <cstddef>
#include <cstdint>
#include <immintrin.h>
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

/* tims unresolved_ijump_handler has been entered */
volatile uint64_t perf_enter_count = 0;

/* number of instruction decoded by the interpreter */
volatile uint64_t perf_instr_count = 0;

/* number of bytes decoded by the interpreter */
volatile uint64_t perf_instr_byte_count = 0;

uint8_t cur_rounding_mode = 0;

void interpreter_dump_perf_stats() {
    puts("perf_enter_count: ");
    print_hex64(perf_enter_count);
    puts("\nperf_instr_count: ");
    print_hex64(perf_instr_count);
    puts("\nperf_instr_byte_count: ");
    print_hex64(perf_instr_byte_count);
    puts("\n");
}

/* for debugging, generates a massive amount of output */
#define TRACE false

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
        /* clear the exception flags (looking at you softfloat) */ \
        register_file[csr_idx] &= 0xE0; \
        break; \
    }

#define FP_THREE_OP_FLOAT() \
    { \
        set_rounding_mode(instr.misc); \
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
        set_rounding_mode(instr.misc); \
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
        set_rounding_mode(instr.misc); \
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
        set_rounding_mode(instr.misc); \
        converter conv1; \
        converter conv2; \
        converter conv3; \
        conv1.d64 = register_file[instr.rs1 + START_FP_STATICS]; \
        conv2.d64 = register_file[instr.rs2 + START_FP_STATICS]; \
        conv3.f64 = operation(conv1.f64, conv2.f64); \
        register_file[instr.rd + (fp_dest_reg ? START_FP_STATICS : 0)] = conv3.d64; \
    }

/* from compiled code */
extern "C" uint64_t register_file[32 + 1 + 32 + 1]; /* size is the maximum register_file size when compiled with floating point support */
extern "C" const uint64_t ijump_lookup_base;
extern "C" const uint64_t ijump_lookup[];
extern "C" const uint64_t ijump_lookup_end;

void trace(uint64_t addr, const FrvInst *instr) {
    puts("TRACE: ");
    print_hex64(addr);
    puts(" : ");

    char buf[32];
    frv_format(instr, sizeof(buf), buf);
    puts(buf);
    puts("\n");
}

uint64_t ijump_lookup_for_addr(uint64_t addr) {
    const uint64_t *const entry = &ijump_lookup[(addr - ijump_lookup_base) / 0x2];
    if (entry >= &ijump_lookup_end) {
        return 0x0;
    } else {
        return *entry;
    }
}

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
    }
}

/**
 * Evalutes what rounding mode must be used in x86_64 for a given RISC-V rounding mode.
 *
 * @param riscv_rounding_mode The rounding mode as defined by RISC-.
 * @param is_rm_field Whether the value is from an instructions rounding mode field.
 * @return uint32_t The corresponding x86_64 rounding mode value.
 */
uint32_t evaluate_rounding_mode(uint32_t riscv_rounding_mode, const bool is_rm_field) {
    switch (riscv_rounding_mode) {
    case 0:
        [[fallthrough]];
    case 4:
        return 0x0000;
    case 1:
        return 0x6000;
    case 2:
        return 0x2000;
    case 3:
        return 0x4000;
    case 7:
        // dynamic rounding mode
        if (is_rm_field) {
            uint32_t fcsr = static_cast<uint32_t>(register_file[FCSR_IDX]);
            // extract rounding mode of fcsr (bits 5-7)
            uint32_t rounding_mode = (fcsr >> 5) & 0x7;
            return evaluate_rounding_mode(rounding_mode, false);
        } else {
            panic("Invalid rounding mode value in csr!");
        }
        break;
    default:
        panic("Discovered unsupported rounding mode!");
        break;
    }
    panic("");
}

void set_rounding_mode(uint8_t rm) {
    if (rm != cur_rounding_mode) {
        uint32_t status = _mm_getcsr();
        // clear rounding mode and set correctly
        status = (status & 0xFF'FF'1F'FF) | evaluate_rounding_mode(rm, true);
        _mm_setcsr(status);
        cur_rounding_mode = rm;
    }
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

    /* floats */
    puts("\nf00: ");
    print_hex64(register_file[START_FP_STATICS + 0]);
    puts("\nf01: ");
    print_hex64(register_file[START_FP_STATICS + 1]);
    puts("\nf02: ");
    print_hex64(register_file[START_FP_STATICS + 2]);
    puts("\nf03: ");
    print_hex64(register_file[START_FP_STATICS + 3]);
    puts("\nf04: ");
    print_hex64(register_file[START_FP_STATICS + 4]);
    puts("\nf05: ");
    print_hex64(register_file[START_FP_STATICS + 5]);
    puts("\nf06: ");
    print_hex64(register_file[START_FP_STATICS + 6]);
    puts("\nf07: ");
    print_hex64(register_file[START_FP_STATICS + 7]);
    puts("\nf08: ");
    print_hex64(register_file[START_FP_STATICS + 8]);
    puts("\nf09: ");
    print_hex64(register_file[START_FP_STATICS + 9]);
    puts("\nf10: ");
    print_hex64(register_file[START_FP_STATICS + 10]);
    puts("\nf11: ");
    print_hex64(register_file[START_FP_STATICS + 11]);
    puts("\nf12: ");
    print_hex64(register_file[START_FP_STATICS + 12]);
    puts("\nf13: ");
    print_hex64(register_file[START_FP_STATICS + 13]);
    puts("\nf14: ");
    print_hex64(register_file[START_FP_STATICS + 14]);
    puts("\nf15: ");
    print_hex64(register_file[START_FP_STATICS + 15]);
    puts("\nf16: ");
    print_hex64(register_file[START_FP_STATICS + 16]);
    puts("\nf17: ");
    print_hex64(register_file[START_FP_STATICS + 17]);
    puts("\nf18: ");
    print_hex64(register_file[START_FP_STATICS + 18]);
    puts("\nf19: ");
    print_hex64(register_file[START_FP_STATICS + 19]);
    puts("\nf20: ");
    print_hex64(register_file[START_FP_STATICS + 20]);
    puts("\nf21: ");
    print_hex64(register_file[START_FP_STATICS + 21]);
    puts("\nf22: ");
    print_hex64(register_file[START_FP_STATICS + 22]);
    puts("\nf23: ");
    print_hex64(register_file[START_FP_STATICS + 23]);
    puts("\nf24: ");
    print_hex64(register_file[START_FP_STATICS + 24]);
    puts("\nf25: ");
    print_hex64(register_file[START_FP_STATICS + 25]);
    puts("\nf26: ");
    print_hex64(register_file[START_FP_STATICS + 26]);
    puts("\nf27: ");
    print_hex64(register_file[START_FP_STATICS + 27]);
    puts("\nf28: ");
    print_hex64(register_file[START_FP_STATICS + 28]);
    puts("\nf29: ");
    print_hex64(register_file[START_FP_STATICS + 29]);
    puts("\nf30: ");
    print_hex64(register_file[START_FP_STATICS + 30]);
    puts("\nf31: ");
    print_hex64(register_file[START_FP_STATICS + 31]);

    puts("\n");
}

/**
 * @param pc unresolved jump target address
 */
extern "C" uint64_t unresolved_ijump_handler(uint64_t pc) {
#if TRACE
    puts("TRACE: enter handler, pc: ");
    print_hex64(pc);
    puts("\n");
    trace_dump_state(pc);
#endif

    perf_enter_count++;
    uint32_t status = _mm_getcsr();
    // clear rounding mode
    status = (status & 0xFF'FF'1F'FF);
    _mm_setcsr(status);
    cur_rounding_mode = 0;
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

#if TRACE
        trace(pc, &instr);
        //    trace_dump_state(pc);
#endif

        perf_instr_count++;
        perf_instr_byte_count += r;

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
                register_file[instr.rd] = register_file[instr.rs1] < static_cast<uint64_t>(sign_extend_int64_t(instr.imm));
            }
            break;
        case FRV_SLT:
            if (instr.rd != 0) {
                register_file[instr.rd] = static_cast<int64_t>(register_file[instr.rs1]) < static_cast<int64_t>(register_file[instr.rs2]);
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
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(static_cast<uint32_t>(register_file[instr.rs1]) << instr.imm));
            }
            break;
        case FRV_SRLIW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(static_cast<uint32_t>(register_file[instr.rs1]) >> instr.imm));
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
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(static_cast<uint32_t>(register_file[instr.rs1]) << (register_file[instr.rs2] & 0x1F)));
            }
            break;
        case FRV_SRLW:
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(static_cast<uint32_t>(register_file[instr.rs1]) >> (register_file[instr.rs2] & 0x1F)));
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
                register_file[instr.rd] = pc + sign_extend_int64_t(instr.imm);
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
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(static_cast<uint32_t>(register_file[instr.rs1]) / static_cast<uint32_t>(register_file[instr.rs2])));
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
                register_file[instr.rd] = sign_extend_int64_t(static_cast<int32_t>(static_cast<uint32_t>(register_file[instr.rs1]) % static_cast<uint32_t>(register_file[instr.rs2])));
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
            set_rounding_mode(instr.misc);
            converter conv1, conv2;
            conv1.d32 = register_file[instr.rs1 + START_FP_STATICS];
            __asm__ __volatile__("sqrtss %1, %0" : "=x"(conv2.d32) : "x"(conv1.f32));
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(conv2.d32);
            break;
        }
        case FRV_FSQRTD: {
            set_rounding_mode(instr.misc);
            converter conv1, conv2;
            conv1.d64 = register_file[instr.rs1 + START_FP_STATICS];
            __asm__ __volatile__("sqrtsd %1, %0" : "=x"(conv2.f64) : "x"(conv1.f64));
            register_file[instr.rd + START_FP_STATICS] = conv2.d64;
            break;
        }

        case FRV_FMADDS: {
            set_rounding_mode(instr.misc);
            converter conv1;
            converter conv2;
            converter conv3;
            conv1.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
            conv2.d32 = static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]);
            conv3.d32 = static_cast<uint32_t>(register_file[instr.rs3 + START_FP_STATICS]);
            if (have_fma) {
                __asm__ __volatile__("vfmadd213ss %2, %1, %0" : "+x"(conv1.f32) : "x"(conv2.f32), "x"(conv3.f32));
            } else {
                conv1.f32 = conv1.f32 * conv2.f32 + conv3.f32;
            }
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(conv1.d32);
            break;
        }
        case FRV_FMSUBS: {
            set_rounding_mode(instr.misc);
            converter conv1;
            converter conv2;
            converter conv3;
            conv1.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
            conv2.d32 = static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]);
            conv3.d32 = static_cast<uint32_t>(register_file[instr.rs3 + START_FP_STATICS]);
            if (have_fma) {
                __asm__ __volatile__("vfmsub213ss %2, %1, %0" : "+x"(conv1.f32) : "x"(conv2.f32), "x"(conv3.f32));
            } else {
                conv1.f32 = conv1.f32 * conv2.f32 - conv3.f32;
            }
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(conv1.d32);
            break;
        }
        case FRV_FNMADDS: {
            set_rounding_mode(instr.misc);
            converter conv1;
            converter conv2;
            converter conv3;
            conv1.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
            conv2.d32 = static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]);
            conv3.d32 = static_cast<uint32_t>(register_file[instr.rs3 + START_FP_STATICS]);
            if (have_fma) {
                __asm__ __volatile__("vfnmsub213ss %2, %1, %0" : "+x"(conv1.f32) : "x"(conv2.f32), "x"(conv3.f32));
            } else {
                conv1.f32 = -(conv1.f32 * conv2.f32 + conv3.f32);
            }
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(conv1.d32);
            break;
        }
        case FRV_FNMSUBS: {
            set_rounding_mode(instr.misc);
            converter conv1;
            converter conv2;
            converter conv3;
            conv1.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
            conv2.d32 = static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]);
            conv3.d32 = static_cast<uint32_t>(register_file[instr.rs3 + START_FP_STATICS]);
            if (have_fma) {
                __asm__ __volatile__("vfnmadd213ss %2, %1, %0" : "+x"(conv1.f32) : "x"(conv2.f32), "x"(conv3.f32));
            } else {
                conv1.f32 = (-conv1.f32) * conv2.f32 + conv3.f32;
            }
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(conv1.d32);
            break;
        }

        case FRV_FMADDD: {
            set_rounding_mode(instr.misc);
            converter conv1;
            converter conv2;
            converter conv3;
            conv1.d64 = register_file[instr.rs1 + START_FP_STATICS];
            conv2.d64 = register_file[instr.rs2 + START_FP_STATICS];
            conv3.d64 = register_file[instr.rs3 + START_FP_STATICS];
            if (have_fma) {
                __asm__ __volatile__("vfmadd213sd %2, %1, %0" : "+x"(conv1.f64) : "x"(conv2.f64), "x"(conv3.f64));
            } else {
                conv1.f64 = conv1.f64 * conv2.f64 + conv3.f64;
            }
            register_file[instr.rd + START_FP_STATICS] = conv1.d64;
            break;
        }
        case FRV_FMSUBD: {
            set_rounding_mode(instr.misc);
            converter conv1;
            converter conv2;
            converter conv3;
            conv1.d64 = register_file[instr.rs1 + START_FP_STATICS];
            conv2.d64 = register_file[instr.rs2 + START_FP_STATICS];
            conv3.d64 = register_file[instr.rs3 + START_FP_STATICS];
            if (have_fma) {
                __asm__ __volatile__("vfmsub213sd %2, %1, %0" : "+x"(conv1.f64) : "x"(conv2.f64), "x"(conv3.f64));
            } else {
                conv1.f64 = conv1.f64 * conv2.f64 + conv3.f64;
            }
            register_file[instr.rd + START_FP_STATICS] = conv1.d64;
            break;
        }
        case FRV_FNMADDD: {
            set_rounding_mode(instr.misc);
            converter conv1;
            converter conv2;
            converter conv3;
            conv1.d64 = register_file[instr.rs1 + START_FP_STATICS];
            conv2.d64 = register_file[instr.rs2 + START_FP_STATICS];
            conv3.d64 = register_file[instr.rs3 + START_FP_STATICS];
            if (have_fma) {
                __asm__ __volatile__("vfnmsub213sd %2, %1, %0" : "+x"(conv1.f64) : "x"(conv2.f64), "x"(conv3.f64));
            } else {
                conv1.f64 = -(conv1.f64 * conv2.f64 + conv3.f64);
            }
            register_file[instr.rd + START_FP_STATICS] = conv1.d64;
            break;
        }
        case FRV_FNMSUBD: {
            set_rounding_mode(instr.misc);
            converter conv1;
            converter conv2;
            converter conv3;
            conv1.d64 = register_file[instr.rs1 + START_FP_STATICS];
            conv2.d64 = register_file[instr.rs2 + START_FP_STATICS];
            conv3.d64 = register_file[instr.rs3 + START_FP_STATICS];
            if (have_fma) {
                __asm__ __volatile__("vfnmadd213sd %2, %1, %0" : "+x"(conv1.f64) : "x"(conv2.f64), "x"(conv3.f64));
            } else {
                conv1.f64 = (-conv1.f64) * conv2.f64 + conv3.f64;
            }
            register_file[instr.rd + START_FP_STATICS] = conv1.d64;
            break;
        }
        case FRV_FCVTWS:
            if (instr.rd != 0) {
                converter conv;
                conv.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
                set_rounding_mode(instr.misc);
                // perform conversion (not using c conversion because this using the conversion with truncation: cvtt)
                int32_t result;
                __asm__ __volatile__("cvtss2si %1, %0" : "=r"(result) : "x"(conv.f32));
                register_file[instr.rd] = static_cast<int64_t>(result);
            }
            break;
        case FRV_FCVTWUS:
            if (instr.rd != 0) {
                converter conv;
                conv.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
                set_rounding_mode(instr.misc);
                // perform conversion (not using c conversion because gcc using the conversion with truncation: cvtt)
                int32_t result;
                __asm__ __volatile__("cvtss2si %1, %0" : "=r"(result) : "x"(conv.f32));
                // if value is negative, the result should be zero. spread sign bit and use inverted value to zero if necessary.
                result = result & ~(conv.i32 >> 31);
                register_file[instr.rd] = static_cast<uint32_t>(result);
            }
            break;
        case FRV_FCVTLS:
            if (instr.rd != 0) {
                converter conv;
                conv.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
                set_rounding_mode(instr.misc);
                // perform conversion (not using c conversion because gcc using the conversion with truncation: cvtt)
                int64_t result;
                __asm__ __volatile__("cvtss2si %1, %0" : "=r"(result) : "x"(conv.f32));
                register_file[instr.rd] = result;
            }
            break;
        case FRV_FCVTLUS:
            if (instr.rd != 0) {
                converter conv;
                conv.d32 = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]);
                set_rounding_mode(instr.misc);
                // perform conversion (not using c conversion because gcc using the conversion with truncation: cvtt)
                int64_t result;
                __asm__ __volatile__("cvtss2si %1, %0" : "=r"(result) : "x"(conv.f32));
                // if value is negative, the result should be zero. spread sign bit and use inverted value to zero if necessary.
                int64_t mask = ~(conv.i64 >> 63);
                result = result & static_cast<int64_t>(mask);
                register_file[instr.rd] = result;
            }
            break;

        case FRV_FCVTSW: {
            set_rounding_mode(instr.misc);
            float res = static_cast<float>(static_cast<int32_t>(register_file[instr.rs1]));
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }
        case FRV_FCVTSWU: {
            set_rounding_mode(instr.misc);
            float res = static_cast<float>(static_cast<uint32_t>(register_file[instr.rs1]));
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }
        case FRV_FCVTSL: {
            set_rounding_mode(instr.misc);
            float res = static_cast<float>(static_cast<int64_t>(register_file[instr.rs1]));
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }
        case FRV_FCVTSLU: {
            set_rounding_mode(instr.misc);
            float res = static_cast<float>(register_file[instr.rs1]);
            uint32_t *res_ptr = reinterpret_cast<uint32_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>(*res_ptr);
            break;
        }

        case FRV_FCVTWD:
            if (instr.rd != 0) {
                converter conv;
                conv.d64 = register_file[instr.rs1 + START_FP_STATICS];
                set_rounding_mode(instr.misc);
                // perform conversion (not using c conversion because this using the conversion with truncation: cvtt)
                int32_t result;
                __asm__ __volatile__("cvtsd2si %1, %0" : "=r"(result) : "x"(conv.f64));
                register_file[instr.rd] = static_cast<int64_t>(result);
            }
            break;
        case FRV_FCVTWUD:
            if (instr.rd != 0) {
                converter conv;
                conv.d64 = register_file[instr.rs1 + START_FP_STATICS];
                set_rounding_mode(instr.misc);
                // perform conversion (not using c conversion because this using the conversion with truncation: cvtt)
                int32_t result;
                __asm__ __volatile__("cvtsd2si %1, %0" : "=r"(result) : "x"(conv.f64));
                // if value is negative, the result should be zero. spread sign bit and use inverted value to zero if necessary.
                result = result & static_cast<int32_t>(~(conv.i64 >> 63));
                register_file[instr.rd] = static_cast<uint32_t>(result);
            }
            break;
        case FRV_FCVTLD:
            if (instr.rd != 0) {
                converter conv;
                conv.d64 = register_file[instr.rs1 + START_FP_STATICS];
                set_rounding_mode(instr.misc);
                // perform conversion (not using c conversion because this using the conversion with truncation: cvtt)
                int64_t result;
                __asm__ __volatile__("cvtsd2si %1, %0" : "=r"(result) : "x"(conv.f64));
                register_file[instr.rd] = result;
            }
            break;
        case FRV_FCVTLUD:
            if (instr.rd != 0) {
                converter conv;
                conv.d64 = register_file[instr.rs1 + START_FP_STATICS];
                set_rounding_mode(instr.misc);
                // perform conversion (not using c conversion because this using the conversion with truncation: cvtt)
                int64_t result;
                __asm__ __volatile__("cvtsd2si %1, %0" : "=r"(result) : "x"(conv.f64));
                // if value is negative, the result should be zero. spread sign bit and use inverted value to zero if necessary.
                result = result & ~(conv.i64 >> 63);
                register_file[instr.rd] = result;
            }
            break;

        case FRV_FCVTDW: {
            set_rounding_mode(instr.misc);
            double res = static_cast<double>(static_cast<int32_t>(register_file[instr.rs1]));
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }
        case FRV_FCVTDWU: {
            set_rounding_mode(instr.misc);
            double res = static_cast<double>(static_cast<uint32_t>(register_file[instr.rs1]));
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }
        case FRV_FCVTDL: {
            set_rounding_mode(instr.misc);
            double res = static_cast<double>(static_cast<int64_t>(register_file[instr.rs1]));
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }
        case FRV_FCVTDLU: {
            set_rounding_mode(instr.misc);
            double res = static_cast<double>(register_file[instr.rs1]);
            uint64_t *res_ptr = reinterpret_cast<uint64_t *>(&res);
            register_file[instr.rd + START_FP_STATICS] = *res_ptr;
            break;
        }
        case FRV_FCVTDS: {
            set_rounding_mode(instr.misc);
            converter conv;
            conv.d32 = register_file[instr.rs1 + START_FP_STATICS];
            conv.f64 = static_cast<double>(conv.f32);

            register_file[instr.rd + START_FP_STATICS] = conv.d64;
            break;
        }
        case FRV_FCVTSD: {
            set_rounding_mode(instr.misc);
            converter conv;
            conv.d64 = register_file[instr.rs1 + START_FP_STATICS];
            conv.f32 = static_cast<float>(conv.f64);

            register_file[instr.rd + START_FP_STATICS] = conv.d32;
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
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1 + START_FP_STATICS];
            } else {
                register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>((static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]) & 0x7FFF'FFFF) |
                                                                                   (static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]) & 0x8000'0000));
            }
            break;
        case FRV_FSGNJD:
            if (instr.rs1 == instr.rs2) {
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1 + START_FP_STATICS];
            } else {
                register_file[instr.rd + START_FP_STATICS] =
                    (register_file[instr.rs1 + START_FP_STATICS] & 0x7FFF'FFFF'FFFF'FFFF) | (register_file[instr.rs2 + START_FP_STATICS] & 0x8000'0000'0000'0000);
            }
            break;
        case FRV_FSGNJNS:
            if (instr.rs1 == instr.rs2) {
                // negate the floating point value (change sign bit)
                register_file[instr.rd + START_FP_STATICS] = static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]) ^ 0x8000'0000;
            } else {
                register_file[instr.rd + START_FP_STATICS] = static_cast<uint64_t>((static_cast<uint32_t>(register_file[instr.rs1 + START_FP_STATICS]) & 0x7FFF'FFFF) |
                                                                                   (~static_cast<uint32_t>(register_file[instr.rs2 + START_FP_STATICS]) & 0x8000'0000));
            }
            break;
        case FRV_FSGNJND:
            if (instr.rs1 == instr.rs2) {
                // negate the floating point value (change sign bit)
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1 + START_FP_STATICS] ^ 0x8000'0000'0000'0000;
            } else {
                register_file[instr.rd + START_FP_STATICS] =
                    (register_file[instr.rs1 + START_FP_STATICS] & 0x7FFF'FFFF'FFFF'FFFF) | (~register_file[instr.rs2 + START_FP_STATICS] & 0x8000'0000'0000'0000);
            }
            break;
        case FRV_FSGNJXS: {
            if (instr.rs1 == instr.rs2) {
                // calculate the absulate value (set sign bit to zero)
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1 + START_FP_STATICS] & 0x7FFF'FFFF;
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
                register_file[instr.rd + START_FP_STATICS] = register_file[instr.rs1 + START_FP_STATICS] & 0x7FFF'FFFF'FFFF'FFFF;
            } else {
                uint64_t new_sign = (register_file[instr.rs1 + START_FP_STATICS] & 0x8000'0000'0000'0000) ^ (register_file[instr.rs2 + START_FP_STATICS] & 0x8000'0000'0000'0000);
                register_file[instr.rd + START_FP_STATICS] = (register_file[instr.rs1 + START_FP_STATICS] & 0x7FFF'FFFF'FFFF'FFFF) | new_sign;
            }
            break;

        // TODO: fix copypasta
        case FRV_FLTS: {
            if (instr.rd != 0) {
                converter conv1, conv2;
                conv1.d32 = register_file[instr.rs1 + START_FP_STATICS];
                conv2.d32 = register_file[instr.rs2 + START_FP_STATICS];
                register_file[instr.rd] = conv1.f32 < conv2.f32 ? 1 : 0;
            }
            break;
        }
        case FRV_FLTD: {
            if (instr.rd != 0) {
                converter conv1, conv2;
                conv1.d64 = register_file[instr.rs1 + START_FP_STATICS];
                conv2.d64 = register_file[instr.rs2 + START_FP_STATICS];
                register_file[instr.rd] = conv1.f64 < conv2.f64 ? 1 : 0;
            }
            break;
        }
        case FRV_FLES: {
            if (instr.rd != 0) {
                converter conv1, conv2;
                conv1.d32 = register_file[instr.rs1 + START_FP_STATICS];
                conv2.d32 = register_file[instr.rs2 + START_FP_STATICS];
                register_file[instr.rd] = conv1.f32 <= conv2.f32 ? 1 : 0;
            }
            break;
        }
        case FRV_FLED: {
            if (instr.rd != 0) {
                converter conv1, conv2;
                conv1.d64 = register_file[instr.rs1 + START_FP_STATICS];
                conv2.d64 = register_file[instr.rs2 + START_FP_STATICS];
                register_file[instr.rd] = conv1.f64 <= conv2.f64 ? 1 : 0;
            }
            break;
        }
        case FRV_FEQS: {
            if (instr.rd != 0) {
                converter conv1, conv2;
                conv1.d32 = register_file[instr.rs1 + START_FP_STATICS];
                conv2.d32 = register_file[instr.rs2 + START_FP_STATICS];
                register_file[instr.rd] = conv1.f32 == conv2.f32 ? 1 : 0;
            }
            break;
        }
        case FRV_FEQD: {
            if (instr.rd != 0) {
                converter conv1, conv2;
                conv1.d64 = register_file[instr.rs1 + START_FP_STATICS];
                conv2.d64 = register_file[instr.rs2 + START_FP_STATICS];
                register_file[instr.rd] = conv1.f64 == conv2.f64 ? 1 : 0;
            }
            break;
        }
        case FRV_FCLASSS:
            if (instr.rd != 0) {
                const uint32_t val = register_file[instr.rs1 + START_FP_STATICS];
                uint64_t result = 0;

                const uint32_t sign = val >> 31;
                const uint32_t exponent = (val >> 23) & 0xFF;
                const uint32_t mantisse = val & 0x7F'FFFF;
                const uint32_t mantisse_msb = mantisse >> 22;

                // negative infinity
                result |= (val == 0xFF80'0000);

                // negative normal number
                result |= (sign && (exponent != 0) && (exponent != 0xFF)) << 1;

                // negative subnormal number
                result |= (sign && (exponent == 0) && (mantisse != 0)) << 2;

                // negative zero
                result |= (val == 0x8000'0000) << 3;

                // positive zero
                result |= (val == 0) << 4;

                // positive subnormal number
                result |= (!sign && (exponent == 0) && (mantisse != 0)) << 5;

                // positive normal number
                result |= (!sign && (exponent != 0) && (exponent != 0xFF)) << 6;

                // positive infinity
                result |= (val == 0x7F80'0000) << 7;

                // sNaN
                result |= ((exponent == 0xFF) && !mantisse_msb && (mantisse != 0)) << 8;

                // qNaN
                result |= ((exponent == 0xFF) && mantisse_msb) << 9;

                register_file[instr.rd] = result;
            }
            break;

        case FRV_FCLASSD:
            if (instr.rd != 0) {
                const uint64_t val = register_file[instr.rs1 + START_FP_STATICS];
                uint64_t result = 0;

                const uint64_t sign = val >> 63;
                const uint64_t exponent = (val >> 52) & 0x7FF;
                const uint64_t mantisse = val & 0xF'FFFF'FFFF'FFFF;
                const uint64_t mantisse_msb = mantisse >> 51;

                // negative infinity
                result |= (val == 0xFFF0'0000'0000'0000);

                // negative normal number
                result |= (sign && (exponent != 0) && (exponent != 0x7FF)) << 1;

                // negative subnormal number
                result |= (sign && (exponent == 0) && (mantisse != 0)) << 2;

                // negative zero
                result |= (val == 0x8000'0000'0000'0000) << 3;

                // positive zero
                result |= (val == 0) << 4;

                // positive subnormal number
                result |= (!sign && (exponent == 0) && (mantisse != 0)) << 5;

                // positive normal number
                result |= (!sign && (exponent != 0) && (exponent != 0x7FF)) << 6;

                // positive infinity
                result |= (val == 0x7FF0'0000'0000'0000) << 7;

                // sNaN
                result |= ((exponent == 0x7FF) && !mantisse_msb && (mantisse != 0)) << 8;

                // qNaN
                result |= ((exponent == 0x7FF) && mantisse_msb) << 9;

                register_file[instr.rd] = result;
            }
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

#if TRACE
    puts("TRACE: found compiled basic block, pc: ");
    print_hex64(pc);
    puts("\n");

    trace_dump_state(pc);

    puts("\n");
#endif

    /* At this point we have found a valid entry point back into
     * the compiled BasicBlocks
     */
    const uint64_t return_addr = ijump_lookup_for_addr(pc);
#if TRACE
    puts("TRACE: leave handler, return_addr: ");
    print_hex64(return_addr);
    puts("\n");
#endif

    return return_addr;
}

} // namespace helper::interpreter
