#pragma once
#include <algorithm>
#include <array>
#include <ostream>

/* [i64, i32, i16, i8]: Integer types, signness is defined by the operation
 * [f64, f32]: Float types
 * [mt]: Memory token. This is NOT an address. Addresses are stored as i64 or i32.
 */
enum class Type { imm, i64, i32, i16, i8, f64, f32, mt };

constexpr std::array<Type, 5> i_cast_order = {Type::i8, Type::i16, Type::i32, Type::i64, Type::imm};
constexpr std::array<Type, 2> f_cast_order = {Type::f32, Type::f64};

/* -1: no simple cast possible (e.g. integer <-> float)
 * 0: t1 !< t2
 * 1: t1 < t2
 */
constexpr int cast_dir(const Type t1, const Type t2) {
    size_t t1_i_idx = i_cast_order.size();
    size_t t2_i_idx = i_cast_order.size();
    for (size_t i = 0; i < i_cast_order.size(); i++) {
        if (i_cast_order[i] == t1) {
            t1_i_idx = i;
        }
        if (i_cast_order[i] == t2) {
            t2_i_idx = i;
        }
    }
    if (t1_i_idx < i_cast_order.size() && t2_i_idx < i_cast_order.size()) {
        return t1_i_idx < t2_i_idx;
    }

    size_t t1_f_idx = f_cast_order.size();
    size_t t2_f_idx = f_cast_order.size();
    for (size_t i = 0; i < f_cast_order.size(); i++) {
        if (f_cast_order[i] == t1) {
            t1_f_idx = i;
        }
        if (f_cast_order[i] == t2) {
            t2_f_idx = i;
        }
    }
    if (t1_f_idx < f_cast_order.size() && t2_f_idx < f_cast_order.size()) {
        return t1_f_idx < t2_f_idx;
    }

    return -1;
}

constexpr bool is_float(const Type type) { return type == Type::f32 || type == Type::f64; }

std::ostream &operator<<(std::ostream &stream, Type type);
