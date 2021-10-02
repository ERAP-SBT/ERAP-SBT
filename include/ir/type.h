#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <ostream>

/* [i64, i32, i16, i8]: Integer types, signness is defined by the operation
 * [f64, f32]: Float types
 * [mt]: Memory token. This is NOT an address. Addresses are stored as i64 or i32.
 */
enum class Type { imm, i64, i32, i16, i8, f64, f32, mt };

// clang-format off
constexpr std::array<std::array<int, 8>, 8> cast_table {{
    {{ 0,  0,  0,  0,  0, -1, -1, -1}},
    {{ 1,  0,  0,  0,  0, -1, -1, -1}},
    {{ 1,  1,  0,  0,  0, -1, -1, -1}},
    {{ 1,  1,  1,  0,  0, -1, -1, -1}},
    {{ 1,  1,  1,  1,  0, -1, -1, -1}},
    {{-1, -1, -1, -1, -1,  0,  0, -1}},
    {{-1, -1, -1, -1, -1,  1,  0, -1}},
    {{-1, -1, -1, -1, -1, -1, -1, -1}}
}};
// clang-format on

/* -1: no simple cast possible (e.g. integer <-> float)
 * 0: t1 !< t2
 * 1: t1 < t2
 */
constexpr int cast_dir(const Type t1, const Type t2) { return cast_table[static_cast<size_t>(t1)][static_cast<size_t>(t2)]; }

constexpr bool is_float(const Type type) { return type == Type::f32 || type == Type::f64; }

constexpr bool is_integer(const Type type) { return type == Type::i8 || type == Type::i16 || type == Type::i32 || type == Type::i64; }

std::ostream &operator<<(std::ostream &stream, Type type);
