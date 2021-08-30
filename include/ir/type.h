#pragma once
#include <algorithm>
#include <array>
#include <ostream>

/* [i64, i32, i16, i8]: Integer types, signness is defined by the operation
 * [f64, f32]: Float types
 * [mt]: Memory token. This is NOT an address. Addresses are stored as i64 or i32.
 */
enum class Type { imm, i64, i32, i16, i8, f64, f32, mt };

constexpr std::array<Type, 4> i_cast_order = {Type::i8, Type::i16, Type::i32, Type::i64};
constexpr std::array<Type, 2> f_cast_order = {Type::f32, Type::f64};

/* -1: no simple cast possible (e.g. integer <-> float)
 * 0: t1 !< t2
 * 1: t1 < t2
 */
int cast_dir(const Type t1, const Type t2);

bool is_floating_point(const Type type) { return type == Type::f32 || type == Type::f64; }

std::ostream &operator<<(std::ostream &stream, Type type);
