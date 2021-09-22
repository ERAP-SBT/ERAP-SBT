#include "frvdec.h"
#include "generator/syscall_ids.h"
#include "generator/x86_64/helper/helper.h"
#include "generator/x86_64/helper/rv64_syscalls.h"

#include <cstddef>
#include <cstdint>

namespace helper::interpreter {

/* from compiled code */
extern "C" uint64_t register_file[];
extern "C" uint64_t ijump_lookup_base;
extern "C" uint64_t ijump_lookup[];

void trace(uint64_t addr, const FrvInst *instr) {
    puts("TRACE: 0x");

    print_hex64(addr);

    puts(" : ");

    char buf[16];
    frv_format(instr, sizeof(buf), buf);
    puts(buf);
    puts("\n");
}

// TODO: bounds check
uint64_t ijump_lookup_for_addr(uint64_t addr) { return ijump_lookup[(addr - ijump_lookup_base) / 0x2]; }

/* make the code a bit clearer */
constexpr uint64_t sign_extend_int64_t(int32_t v) { return static_cast<int64_t>(v); }

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
            panic("Unable to decode instruction");
        } else if (r == FRV_PARTIAL) {
            panic("partial instruction");
        } else if (r < 0) {
            panic("undefined");
        }

        trace(pc, &instr);

        // trace_dump_state(pc);

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
        case FRV_AMOSWAPW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            *ptr = static_cast<int32_t>(register_file[instr.rs2]);
            break;
        }
        case FRV_AMOSWAPD: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            *ptr = register_file[instr.rs2];
            break;
        }
        case FRV_AMOADDW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            *ptr = static_cast<int32_t>(register_file[instr.rd] + static_cast<int32_t>(register_file[instr.rs2]));
            break;
        }
        case FRV_AMOADDD: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            *ptr = register_file[instr.rd] + register_file[instr.rs2];
            break;
        }
        case FRV_AMOANDW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            *ptr = static_cast<int32_t>(register_file[instr.rd] & static_cast<int32_t>(register_file[instr.rs2]));
            break;
        }
        case FRV_AMOANDD: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            *ptr = register_file[instr.rd] & register_file[instr.rs2];
            break;
        }
        case FRV_AMOORW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            *ptr = static_cast<int32_t>(register_file[instr.rd] | static_cast<int32_t>(register_file[instr.rs2]));
            break;
        }
        case FRV_AMOORD: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            *ptr = register_file[instr.rd] | register_file[instr.rs2];
            break;
        }
        case FRV_AMOXORW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            *ptr = static_cast<int32_t>(register_file[instr.rd] ^ static_cast<int32_t>(register_file[instr.rs2]));
            break;
        }
        case FRV_AMOXORD: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            *ptr = register_file[instr.rd] ^ register_file[instr.rs2];
            break;
        }
        case FRV_AMOMAXW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            int32_t dest_val = static_cast<int32_t>(register_file[instr.rd]);
            int32_t source_val = static_cast<int32_t>(register_file[instr.rs2]);
            *ptr = (dest_val > source_val) ? dest_val : source_val;
            break;
        }
        case FRV_AMOMAXD: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            int64_t dest_val = static_cast<int64_t>(register_file[instr.rd]);
            int64_t source_val = static_cast<int64_t>(register_file[instr.rs2]);
            *ptr = (dest_val > source_val) ? dest_val : source_val;
            break;
        }
        case FRV_AMOMAXUW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            uint32_t dest_val = static_cast<uint32_t>(register_file[instr.rd]);
            uint32_t source_val = static_cast<uint32_t>(register_file[instr.rs2]);
            *ptr = (dest_val > source_val) ? dest_val : source_val;
            break;
        }
        case FRV_AMOMAXUD: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            *ptr = (register_file[instr.rd] > register_file[instr.rs2]) ? register_file[instr.rd] : register_file[instr.rs2];
            break;
        }
        case FRV_AMOMINW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            int32_t dest_val = static_cast<int32_t>(register_file[instr.rd]);
            int32_t source_val = static_cast<int32_t>(register_file[instr.rs2]);
            *ptr = (dest_val < source_val) ? dest_val : source_val;
            break;
        }
        case FRV_AMOMIND: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            int64_t dest_val = static_cast<int64_t>(register_file[instr.rd]);
            int64_t source_val = static_cast<int64_t>(register_file[instr.rs2]);
            *ptr = (dest_val < source_val) ? dest_val : source_val;
            break;
        }
        case FRV_AMOMINUW: {
            int32_t *ptr = reinterpret_cast<int32_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = sign_extend_int64_t(*ptr);
            }
            uint32_t dest_val = static_cast<uint32_t>(register_file[instr.rd]);
            uint32_t source_val = static_cast<uint32_t>(register_file[instr.rs2]);
            *ptr = (dest_val < source_val) ? dest_val : source_val;
            break;
        }
        case FRV_AMOMINUD: {
            int64_t *ptr = reinterpret_cast<int64_t *>(register_file[instr.rs1]);
            if (instr.rd != 0) {
                register_file[instr.rd] = *ptr;
            }
            *ptr = (register_file[instr.rd] < register_file[instr.rs2]) ? register_file[instr.rd] : register_file[instr.rs2];
            break;
        }

            /* F extension */

        default:
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