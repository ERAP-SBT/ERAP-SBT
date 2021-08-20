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
    setup_stack,
    umax, // dst, cmp1, cmp2
    umin, // dst, cmp1, cmp2
    max,  // signed max
    min,  // signed min
    fmul,    // floating point multiplication
    fsqrt,   // floating point square root
    fmin,    // floating point minimum
    fmax,    // floating point maximum
    ffmadd,  // floating point fused multiply add, d = a * b + c
    ffmsub,  // floating point fused multiply sub, d = a * b - c 
    ffnmadd, // floating point fused negative multiply add, d = - (a * b) + c
    ffnmsub, // floating point fused negative multiply sub, d = - (a * b) - c
    convert, // conversion between integer and floating point or between single and double precision
    uconvert // conversion between unsigned integer and floating point
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
