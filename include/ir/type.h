#pragma once

/* [i64, i32, i16, i8]: Integer types, signness is defined by the operation
 * [f64, f32]: Float types
 * [m64, m32]: Memory tokens. These are NOT addresses. Addresses are stored as i64 or i32.
 */
enum class Type
{
    i64,
    i32,
    i16,
    i8,
    f64,
    f32,
    m64,
    m32,
};