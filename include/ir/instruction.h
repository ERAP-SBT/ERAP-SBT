#pragma once
#include <ostream>

enum class Instruction {
    store,
    load,
    add,
    sub,
    mul,
    umul,
    div,
    udiv,
    shl,
    shr,
    sar,
    _or,
    _and,
    _not,
    _xor,
    cast,
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
