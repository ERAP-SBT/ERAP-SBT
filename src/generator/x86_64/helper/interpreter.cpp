#include "generator/syscall_ids.h"
#include "generator/x86_64/helper/helper.h"
#include "generator/x86_64/helper/rv64_syscalls.h"

#include <cstdint>
#include <cstddef>

#include "frvdec.h"

namespace helper::interpreter {

void trace(uint64_t addr, const FrvInst* instr) {
    puts("TRACE: 0x");

    print_hex64(addr);

    puts(" : ");

    char buf[16];
    frv_format(instr, sizeof(buf), buf);
    puts(buf);
    puts("\n");
}

/* from compiled code */
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

uint64_t ijump_lookup_for_addr(uint64_t addr) {
    return ijump_lookup[(addr - ijump_lookup_base) / 0x2];
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

    panic("test");
}

#if 0
void trace(FrvInst instr) {
    helper::syscall1
}
#endif

}
