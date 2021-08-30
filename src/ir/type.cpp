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

int cast_dir(const Type t1, const Type t2) {
    size_t t1_i_idx = std::distance(i_cast_order.begin(), std::find(i_cast_order.begin(), i_cast_order.end(), t1));
    size_t t2_i_idx = std::distance(i_cast_order.begin(), std::find(i_cast_order.begin(), i_cast_order.end(), t2));
    if (t1_i_idx < i_cast_order.size() && t2_i_idx < i_cast_order.size()) {
        return t1_i_idx < t2_i_idx;
    }

    size_t t1_f_idx = std::distance(f_cast_order.begin(), std::find(f_cast_order.begin(), f_cast_order.end(), t1));
    size_t t2_f_idx = std::distance(f_cast_order.begin(), std::find(f_cast_order.begin(), f_cast_order.end(), t2));
    if (t1_f_idx < f_cast_order.size() && t2_f_idx < f_cast_order.size()) {
        return t1_f_idx < t2_f_idx;
    }

    // no cast from integer to float or vice versa
    if ((t1_i_idx < i_cast_order.size() && t2_f_idx < f_cast_order.size()) || (t1_f_idx < f_cast_order.size() && t2_i_idx < i_cast_order.size())) {
        return 0;
    }

    return -1;
}

bool type_is_floating_point(const Type type) { return type == Type::f32 || type == Type::f64; }
