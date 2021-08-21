#include "ir/instruction.h"

std::ostream &operator<<(std::ostream &stream, Instruction instr) {
    switch (instr) {
    case Instruction::store:
        stream << "store";
        break;
    case Instruction::load:
        stream << "load";
        break;
    case Instruction::add:
        stream << "add";
        break;
    case Instruction::sub:
        stream << "sub";
        break;
    case Instruction::mul_l:
        stream << "mul_l";
        break;
    case Instruction::ssmul_h:
        stream << "ssmul_h";
        break;
    case Instruction::uumul_h:
        stream << "uumul_h";
        break;
    case Instruction::sumul_h:
        stream << "sumul_h";
        break;
    case Instruction::div:
        stream << "div";
        break;
    case Instruction::udiv:
        stream << "udiv";
        break;
    case Instruction::shl:
        stream << "shl";
        break;
    case Instruction::shr:
        stream << "shr";
        break;
    case Instruction::sar:
        stream << "sar";
        break;
    case Instruction::_or:
        stream << "or";
        break;
    case Instruction::_and:
        stream << "and";
        break;
    case Instruction::_not:
        stream << "not";
        break;
    case Instruction::_xor:
        stream << "xor";
        break;
    case Instruction::cast:
        stream << "cast";
        break;
    case Instruction::setup_stack:
        stream << "setup_stack";
        break;
    case Instruction::zero_extend:
        stream << "zext";
        break;
    case Instruction::sign_extend:
        stream << "sext";
        break;
    case Instruction::slt:
        stream << "slt";
        break;
    case Instruction::sltu:
        stream << "sltu";
        break;
    case Instruction::umax:
        stream << "umax";
        break;
    case Instruction::umin:
        stream << "umin";
        break;
    case Instruction::max:
        stream << "max";
        break;
    case Instruction::min:
        stream << "min";
        break;
    case Instruction::fmul: 
        stream << "fmul";
        break;
    case Instruction::fsqrt:
        stream << "fsqrt";
        break;
    case Instruction::fmin:
        stream << "fmin";
        break;
    case Instruction::fmax:
        stream << "fmax";
        break;
    case Instruction::ffmadd:
        stream << "ffmadd";
        break;
    case Instruction::ffmsub:
        stream << "ffmsub";
        break;
    case Instruction::ffnmadd:
        stream << "ffnmadd";
        break;
    case Instruction::ffnmsub:
        stream << "ffnmsub";
        break;
    case Instruction::fle:
        stream << "fle";
        break;
    case Instruction::flt:
        stream << "flt";
        break;
    case Instruction::feq:
        stream << "feq";
        break;
    case Instruction::convert:
        stream << "convert";
        break;
    case Instruction::uconvert:
        stream << "uconvert";
        break;
    }
    return stream;
}

std::ostream &operator<<(std::ostream &stream, CFCInstruction instr) {
    switch (instr) {
    case CFCInstruction::jump:
        stream << "jump";
        break;
    case CFCInstruction::ijump:
        stream << "ijump";
        break;
    case CFCInstruction::cjump:
        stream << "cjump";
        break;
    case CFCInstruction::call:
        stream << "call";
        break;
    case CFCInstruction::icall:
        stream << "icall";
        break;
    case CFCInstruction::_return:
        stream << "return";
        break;
    case CFCInstruction::unreachable:
        stream << "unreachable";
        break;
    case CFCInstruction::syscall:
        stream << "syscall";
        break;
    }

    return stream;
}
