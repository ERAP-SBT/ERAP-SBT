#pragma once

enum class Instruction
{
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
    immediate,
    setup_stack,
};

// CFC <=> Control Flow Change
enum class CFCInstruction
{
    jump,
    ijump,
    cjump,
    call,
    icall,
    _return,
    unreachable,
    syscall,
};