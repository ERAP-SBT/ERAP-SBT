#include "ir/type.h"

std::ostream &operator<<(std::ostream &stream, Type type) {
    switch (type) {
    case Type::i64:
        stream << "i64";
        break;
    case Type::i32:
        stream << "i32";
        break;
    case Type::i16:
        stream << "i16";
        break;
    case Type::i8:
        stream << "i8";
        break;
    case Type::f64:
        stream << "f64";
        break;
    case Type::f32:
        stream << "f32";
        break;
    case Type::mt:
        stream << "mt";
        break;
    case Type::imm:
        stream << "imm";
        break;
    }

    return stream;
}
