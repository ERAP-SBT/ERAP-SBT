#pragma once
#include <ostream>

enum class Instruction {
    store, // mt_out, addr, val, mt_in
    load, // dst, addr, mem_token
    add, // dst, op1, op2
    sub, // dst, op1, op2: op1 - op2
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
    cast, // used for shrinking size of an integer variable or to cast the bits from floating point to integer or vice versa or for shrinking the f32 values which are zero extended to f64
    slt,  // set if less than, (v1 < v2) ? v3 : v4
    sltu, // set if less than unsigned, (v1 <u v2) ? v3 : v4
    sle,  // set if less than or equal, (v1 <= v2) ? v3 : v4
    seq,  // set if equals, (v1 == v2) ? v3 : v4
    sign_extend,
    zero_extend,
    setup_stack,
    umax,     // dst, cmp1, cmp2
    umin,     // dst, cmp1, cmp2
    max,      // signed max
    min,      // signed min
    fmul,     // floating point multiplication, c = a * b
    fdiv,     // floating point division, c = a / b
    fsqrt,    // floating point square root, c = sqrt(a)
    fmadd,    // fused multiply add, d = a * b + c
    fmsub,    // fused multiply sub, d = a * b - c
    fnmadd,   // fused negative multiply add, d = - (a * b) + c
    fnmsub,   // fused negative multiply sub, d = - (a * b) - c
    convert,  // conversion between integer and floating point or between single and double precision (maybe with rounding_mode)
    uconvert, // conversion between unsigned integer and floating point (maybe with rounding_mode)
};

enum class RoundingMode {
    ZERO,    // Round towards zero
    NEAREST, // Round to nearest (ties to even)
    DOWN,    // Round down, to -inf
    UP       // Round up, to +inf
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
