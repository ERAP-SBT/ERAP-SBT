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
    fmin,     // floating point minimum, c = min(a, b)
    fmax,     // floating point maximum, c = max(a, b)
    ffmadd,   // floating point fused multiply add, d = a * b + c
    ffmsub,   // floating point fused multiply sub, d = a * b - c
    ffnmadd,  // floating point fused negative multiply add, d = - (a * b) + c
    convert,  // conversion between integer and floating point or between single and double precision: input = {value, rounding_mode}
    uconvert, // conversion between unsigned integer and floating point: input = {value, rounding_mode}
};

enum class RoundingMode {
    RZERO,    // Round towards zero
    RNEAREST, // Round to nearest (ties to even)
    RDOWN,    // Round down, to -inf
    RUP       // Round up, to +inf
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
