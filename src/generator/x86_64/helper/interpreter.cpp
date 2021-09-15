#include "generator/syscall_ids.h"
#include "generator/x86_64/helper/helper.h"
#include "generator/x86_64/helper/rv64_syscalls.h"

#include <cstdint>
#include <cstddef>

#include "frvdec.h"

namespace helper::interpreter {

/* from compiled code */
extern "C" uint64_t register_file[];
extern "C" uint64_t s0;
extern "C" uint64_t s1;
extern "C" uint64_t s2;
extern "C" uint64_t s3;
extern "C" uint64_t s4;
extern "C" uint64_t s5;
extern "C" uint64_t s6;
extern "C" uint64_t s7;
extern "C" uint64_t s8;
extern "C" uint64_t s9;
extern "C" uint64_t s10;
extern "C" uint64_t s11;
extern "C" uint64_t s12;
extern "C" uint64_t s13;
extern "C" uint64_t s14;
extern "C" uint64_t s15;
extern "C" uint64_t s16;
extern "C" uint64_t s17;
extern "C" uint64_t s18;
extern "C" uint64_t s19;
extern "C" uint64_t s20;
extern "C" uint64_t s21;
extern "C" uint64_t s22;
extern "C" uint64_t s23;
extern "C" uint64_t s24;
extern "C" uint64_t s25;
extern "C" uint64_t s26;
extern "C" uint64_t s27;
extern "C" uint64_t s28;
extern "C" uint64_t s29;
extern "C" uint64_t s30;
extern "C" uint64_t s31;
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

void trace_dump_state(void) {
    puts("TRACE: STATE");

    puts("\ns00: "); print_hex64(s0);
    puts("\ns01: "); print_hex64(s1);
    puts("\ns02: "); print_hex64(s2);
    puts("\ns03: "); print_hex64(s3);
    puts("\ns04: "); print_hex64(s4);
    puts("\ns05: "); print_hex64(s5);
    puts("\ns06: "); print_hex64(s6);
    puts("\ns07: "); print_hex64(s7);
    puts("\ns08: "); print_hex64(s8);
    puts("\ns09: "); print_hex64(s9);
    puts("\ns10: "); print_hex64(s10);
    puts("\ns11: "); print_hex64(s11);
    puts("\ns12: "); print_hex64(s12);
    puts("\ns13: "); print_hex64(s13);
    puts("\ns14: "); print_hex64(s14);
    puts("\ns15: "); print_hex64(s15);
    puts("\ns16: "); print_hex64(s16);
    puts("\ns17: "); print_hex64(s17);
    puts("\ns18: "); print_hex64(s18);
    puts("\ns19: "); print_hex64(s19);
    puts("\ns20: "); print_hex64(s20);
    puts("\ns21: "); print_hex64(s21);
    puts("\ns22: "); print_hex64(s22);
    puts("\ns23: "); print_hex64(s23);
    puts("\ns24: "); print_hex64(s24);
    puts("\ns25: "); print_hex64(s25);
    puts("\ns26: "); print_hex64(s26);
    puts("\ns27: "); print_hex64(s27);
    puts("\ns28: "); print_hex64(s28);
    puts("\ns29: "); print_hex64(s29);
    puts("\ns30: "); print_hex64(s30);
    puts("\ns31: "); print_hex64(s31);

    puts("\n");
}

/**
 * @param target unresolved jump target address
 */
extern "C" uint64_t unresolved_ijump(uint64_t target) {
    puts("TRACE: enter target: ");
    print_hex64(target);
    puts("\n");

    FrvInst instr;
    (void)frv_decode(0x4, reinterpret_cast<const uint8_t*>(target), FRV_RV64, &instr);

    trace(target, &instr);

//    print_hex64(ijump_lookup[]

    puts("\n");
    print_hex64(ijump_lookup_base);
    puts("\n");
    const uint64_t target_i = target - ijump_lookup_base;
    print_hex64(target_i);
    puts("\n");

#if 0
    for (uint64_t i = 0; i < 64; i++) {
        print_hex64(ijump_lookup[i]);
        puts("\n");
    }
#endif
    puts("\n");
    print_hex64(ijump_lookup_for_addr(target));
    puts("\n");
    print_hex64(s10);
    puts("\n");
    print_hex64(s11);
    puts("\n");
    print_hex64(s12);
    puts("\n");
    print_hex64(s13);
    puts("\n");
    print_hex64(s14);
    puts("\n");
    print_hex64(s15);
    puts("\n");

    trace_dump_state();

    puts("\n");

    if (instr.mnem == FRV_ADDI) {
        // FIXME: sign extend, handle x0
        register_file[instr.rd] = register_file[instr.rs1] + instr.imm;
    }

    puts("\n");

    trace_dump_state();

    panic("test");
}

#if 0
void trace(FrvInst instr) {
    helper::syscall1
}
#endif

}
