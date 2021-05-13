#include "ir/type.h"

std::ostream &operator<<(std::ostream &stream, Type type)
{
    switch (type)
    {
    case Type::i64: stream << "i64"; break;
    case Type::i32: stream << "i32"; break;
    case Type::i16: stream << "i16"; break;
    case Type::i8: stream << "i8"; break;
    case Type::f64: stream << "f64"; break;
    case Type::f32: stream << "f32"; break;
    case Type::m64: stream << "m64"; break;
    case Type::m32: stream << "m32"; break;
    case Type::imm: stream << "imm"; break;
    default: stream << "unk"; break;
    }

    return stream;
}