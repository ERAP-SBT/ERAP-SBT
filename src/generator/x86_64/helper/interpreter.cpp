#include "generator/syscall_ids.h"
#include "generator/x86_64/helper/helper.h"
#include "generator/x86_64/helper/rv64_syscalls.h"

#include <cstdint>
#include <cstddef>

#include "frvdec.h"

namespace helper::interpreter {

/* from compiled code */
extern "C" uint64_t register_file[];
extern "C" uint64_t ijump_lookup_base;
extern "C" uint64_t ijump_lookup[];

void trace(uint64_t addr, const FrvInst* instr) {
    puts("TRACE: 0x");

    print_hex64(addr);

    puts(" : ");

    char buf[16];
    frv_format(instr, sizeof(buf), buf);
    puts(buf);
    puts("\n");
}

uint64_t ijump_lookup_for_addr(uint64_t addr) {
    return ijump_lookup[(addr - ijump_lookup_base) / 0x2];
}

void trace_dump_state(uint64_t pc) {
    puts("TRACE: STATE");

    puts("\npc:  "); print_hex64(pc);
    puts("\ns00: "); print_hex64(register_file[0]);
    puts("\ns01: "); print_hex64(register_file[1]);
    puts("\ns02: "); print_hex64(register_file[2]);
    puts("\ns03: "); print_hex64(register_file[3]);
    puts("\ns04: "); print_hex64(register_file[4]);
    puts("\ns05: "); print_hex64(register_file[5]);
    puts("\ns06: "); print_hex64(register_file[6]);
    puts("\ns07: "); print_hex64(register_file[7]);
    puts("\ns08: "); print_hex64(register_file[8]);
    puts("\ns09: "); print_hex64(register_file[9]);
    puts("\ns10: "); print_hex64(register_file[10]);
    puts("\ns11: "); print_hex64(register_file[11]);
    puts("\ns12: "); print_hex64(register_file[12]);
    puts("\ns13: "); print_hex64(register_file[13]);
    puts("\ns14: "); print_hex64(register_file[14]);
    puts("\ns15: "); print_hex64(register_file[15]);
    puts("\ns16: "); print_hex64(register_file[16]);
    puts("\ns17: "); print_hex64(register_file[17]);
    puts("\ns18: "); print_hex64(register_file[18]);
    puts("\ns19: "); print_hex64(register_file[19]);
    puts("\ns20: "); print_hex64(register_file[20]);
    puts("\ns21: "); print_hex64(register_file[21]);
    puts("\ns22: "); print_hex64(register_file[22]);
    puts("\ns23: "); print_hex64(register_file[23]);
    puts("\ns24: "); print_hex64(register_file[24]);
    puts("\ns25: "); print_hex64(register_file[25]);
    puts("\ns26: "); print_hex64(register_file[26]);
    puts("\ns27: "); print_hex64(register_file[27]);
    puts("\ns28: "); print_hex64(register_file[28]);
    puts("\ns29: "); print_hex64(register_file[29]);
    puts("\ns30: "); print_hex64(register_file[30]);
    puts("\ns31: "); print_hex64(register_file[31]);

    puts("\n");
}

/**
 * @param target unresolved jump target address
 */
extern "C" uint64_t unresolved_ijump_handler(uint64_t target) {
    puts("TRACE: handler, target: "); print_hex64(target); puts("\n");

    uint64_t pc = target;

    do {
        FrvInst instr;
        const int r = frv_decode(0x1000, reinterpret_cast<const uint8_t*>(pc), FRV_RV64, &instr);

        if (r == FRV_UNDEF) {
            panic("Unable to decode instruction");
        } else if (r == FRV_PARTIAL) {
            panic("partial instruction");
        } else if (r < 0) {
            panic("undefined");
        }

        trace(pc, &instr);

        trace_dump_state(pc);

        if (instr.mnem == FRV_ADDI) {
            if (instr.rd != 0) {
                // FIXME: sign extend, handle x0
                print_hex64(register_file[instr.rs1]); puts(" ");
                print_hex64(instr.imm); puts(" ");
                register_file[instr.rd] = register_file[instr.rs1] + instr.imm;
            }
        } else if (instr.mnem == FRV_ADD) {
            if (instr.rd != 0) {
                print_hex64(register_file[instr.rs1]); puts(" ");
                print_hex64(register_file[instr.rs2]); puts(" ");
                register_file[instr.rd] = register_file[instr.rs1] + register_file[instr.rs2];
            }
        } else {
            panic("instruction not implemented");
        }

        pc += r; // FIXME: is increment PC a pre or post operation ?
    } while (ijump_lookup_for_addr(pc) == 0);

    puts("TRACE: found compiled basic block, addr: "); print_hex64(pc); puts("\n");

    trace_dump_state(pc);

    puts("\n");

    /* At this point we have found a valid entry point back into
     * the compiled BasicBlocks
     */
    return ijump_lookup_for_addr(pc);
}

}
