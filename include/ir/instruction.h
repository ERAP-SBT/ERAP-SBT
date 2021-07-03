#pragma once
#include <ostream>

enum class Instruction {
    store,
    load,
    add,
    sub,
    mul_l,   //                         => lower half of result
    ssmul_h, // signed * signed         => upper half of result
    uumul_h, // unsigned * unsigned     => upper half of result
    sumul_h, // signed * unsigned       => upper half of result
    div,     // two possible return values: 1. result, 2. remainder
    udiv,
    shl,
    shr,
    sar,
    _or,
    _and,
    _not,
    _xor,
    cast,
    slt,
    sltu,
    sign_extend,
    zero_extend,
    immediate,
    setup_stack,
};

std::ostream &operator<<(std::ostream &stream, Instruction instr);

// CFC <=> Control Flow Change
enum class CFCInstruction {
    jump,
    ijump,
    cjump,
    call,
    icall,
    _return,
    unreachable,
    syscall,
};

std::ostream &operator<<(std::ostream &stream, CFCInstruction instr);
