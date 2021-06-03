#pragma once
#include <ostream>

/* [i64, i32, i16, i8]: Integer types, signness is defined by the operation
 * [f64, f32]: Float types
 * [mt]: Memory token. This is NOT an address. Addresses are stored as i64 or i32.
 */
enum class Type {
    i64 = 3,
    i32 = 2,
    i16 = 1,
    i8 = 0,
    f64 = 5,
    f32 = 4,
    mt = 6,
    imm = 7,
};

std::ostream &operator<<(std::ostream &stream, Type type);
