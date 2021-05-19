#include "ir/instruction.h"

std::ostream &operator<<(std::ostream &stream, Instruction instr)
{
    switch (instr)
    {
    case Instruction::store: stream << "store"; break;
    case Instruction::load: stream << "load"; break;
    case Instruction::add: stream << "add"; break;
    case Instruction::sub: stream << "sub"; break;
    case Instruction::mul: stream << "mul"; break;
    case Instruction::umul: stream << "umul"; break;
    case Instruction::div: stream << "div"; break;
    case Instruction::udiv: stream << "udiv"; break;
    case Instruction::shl: stream << "shl"; break;
    case Instruction::shr: stream << "shr"; break;
    case Instruction::sar: stream << "sar"; break;
    case Instruction::_or: stream << "or"; break;
    case Instruction::_and: stream << "and"; break;
    case Instruction::_not: stream << "not"; break;
    case Instruction::_xor: stream << "xor"; break;
    case Instruction::cast: stream << "cast"; break;
    case Instruction::immediate: stream << "immediate"; break;
    case Instruction::setup_stack: stream << "setup_stack"; break;
    }

    return stream;
}

std::ostream &operator<<(std::ostream &stream, CFCInstruction instr)
{
    switch (instr)
    {
    case CFCInstruction::jump: stream << "jump"; break;
    case CFCInstruction::ijump: stream << "ijump"; break;
    case CFCInstruction::cjump: stream << "cjump"; break;
    case CFCInstruction::call: stream << "call"; break;
    case CFCInstruction::icall: stream << "icall"; break;
    case CFCInstruction::_return: stream << "return"; break;
    case CFCInstruction::unreachable: stream << "unreachable"; break;
    case CFCInstruction::syscall: stream << "syscall"; break;
    }

    return stream;
}
