#include "ir/eval.h"

#include "ir/optimizer/common.h"

// Type helpers

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
using uint128 = __int128;
using int128 = signed __int128;
#pragma GCC diagnostic pop

template <typename T> struct Int;
template <> struct Int<uint8_t> {
    using SignedType = int8_t;
    using NextLarger = uint16_t;
};
template <> struct Int<uint16_t> {
    using SignedType = int16_t;
    using NextLarger = uint32_t;
};
template <> struct Int<uint32_t> {
    using SignedType = int32_t;
    using NextLarger = uint64_t;
};
template <> struct Int<uint64_t> {
    using SignedType = int64_t;
    using NextLarger = uint128;
};
template <> struct Int<uint128> { using SignedType = int128; };

template <typename T> using SignedIntT = typename Int<T>::SignedType;
template <typename T> using UnsignedIntT = typename Int<T>::UnsignedType;
template <typename T> using NextLargerT = typename Int<T>::NextLarger;
template <typename T> constexpr size_t bit_count = sizeof(T) * 8;

#define switch_int_types(tpy) \
    switch (tpy) { \
    case Type::i8: \
        switch_action(uint8_t); \
    case Type::i16: \
        switch_action(uint16_t); \
    case Type::i32: \
        switch_action(uint32_t); \
    case Type::i64: \
        switch_action(uint64_t); \
    default: \
        panic("Non-int type"); \
        break; \
    }

namespace {
template <typename From, typename To> inline To eval_morphing_op3(Instruction insn, From val) {
    using SignedTo = SignedIntT<To>;
    using SignedFrom = SignedIntT<From>;

    switch (insn) {
    case Instruction::sign_extend:
        if constexpr (sizeof(From) > sizeof(To)) {
            panic("sign_extend from larger to smaller type not permitted");
        } else {
            return (To)(SignedTo)(SignedFrom)val;
        }
    case Instruction::zero_extend:
        if constexpr (sizeof(From) > sizeof(To)) {
            panic("zero_extend from larger to smaller type not permitted");
        } else {
            return (To)val;
        }
    case Instruction::cast:
        if constexpr (sizeof(From) < sizeof(To)) {
            panic("cast from smaller to larger type not permitted");
        } else {
            return (To)val;
        }
    default:
        unreachable();
    }
}

template <typename From> inline uint64_t eval_morphing_op2(Instruction insn, Type to, From raw) {
#define switch_action(sw_typ) return (uint64_t)eval_morphing_op3<From, sw_typ>(insn, raw)
    switch_int_types(to)
#undef switch_action
}
} // namespace

uint64_t eval_morphing_op(Instruction insn, Type from, Type to, uint64_t raw) {
#define switch_action(sw_typ) return eval_morphing_op2<sw_typ>(insn, to, (sw_typ)raw)
    switch_int_types(from)
#undef switch_action
}

namespace {
template <typename T> inline T eval_binary_op2(Instruction insn, T a, T b) {
    using Signed = SignedIntT<T>;
    using NextLarger = NextLargerT<T>;
    using SignedNextLarger = SignedIntT<NextLarger>;

    switch (insn) {
    case Instruction::add:
        return a + b;
    case Instruction::sub:
        return a - b;
    case Instruction::mul_l:
        return a * b;
    case Instruction::ssmul_h: {
        auto ax = (SignedNextLarger)(Signed)a;
        auto bx = (SignedNextLarger)(Signed)b;
        auto large_result = (NextLarger)(ax * bx);
        return (T)(large_result >> bit_count<T>);
    }
    case Instruction::uumul_h: {
        auto ax = (NextLarger)a;
        auto bx = (NextLarger)b;
        auto large_result = ax * bx;
        return (T)(large_result >> bit_count<T>);
    }
    case Instruction::sumul_h: {
        auto ax = (SignedNextLarger)(Signed)a;
        auto bx = (NextLarger)b;
        auto large_result = (NextLarger)(ax * bx);
        return (T)(large_result >> bit_count<T>);
    }
    case Instruction::shl:
        return a << b;
    case Instruction::shr:
        return a >> b;
    case Instruction::sar:
        return (Signed)a >> b;
    case Instruction::_or:
        return a | b;
    case Instruction::_and:
        return a & b;
    case Instruction::_xor:
        return a ^ b;
    case Instruction::umax:
        return std::max(a, b);
    case Instruction::max:
        return std::max((Signed)a, (Signed)b);
    case Instruction::umin:
        return std::min(a, b);
    case Instruction::min:
        return std::min((Signed)a, (Signed)b);
    default:
        unreachable();
    }
}
} // namespace

uint64_t eval_binary_op(Instruction insn, Type type, uint64_t a, uint64_t b) {
#define switch_action(sw_typ) return (uint64_t)eval_binary_op2<sw_typ>(insn, (sw_typ)a, (sw_typ)b)
    switch_int_types(type)
#undef switch_action
}

namespace {
template <typename T> inline std::pair<T, T> eval_div2(Instruction insn, T a, T b) {
    using Signed = SignedIntT<T>;

    switch (insn) {
    case Instruction::div: {
        auto div = (Signed)a / (Signed)b;
        auto rem = (Signed)a % (Signed)b;
        return std::make_pair(div, rem);
    }
    case Instruction::udiv: {
        auto div = a / b;
        auto rem = a % b;
        return std::make_pair(div, rem);
    }
    default:
        unreachable();
    }
}
} // namespace

std::pair<uint64_t, uint64_t> eval_div(Instruction insn, Type type, uint64_t a, uint64_t b) {
#define switch_action(sw_typ) \
    { \
        auto [div, rem] = eval_div2(insn, (sw_typ)a, (sw_typ)b); \
        return std::make_pair((uint64_t)div, (uint64_t)rem); \
    }
    switch_int_types(type)
#undef switch_action
}

namespace {
template <typename T> inline T eval_unary_op2(Instruction insn, T val) {
    switch (insn) {
    case Instruction::_not:
        return ~val;
    default:
        unreachable();
    }
}
} // namespace

uint64_t eval_unary_op(Instruction insn, Type type, uint64_t val) {
#define switch_action(sw_typ) return (uint64_t)eval_unary_op2<sw_typ>(insn, (sw_typ)val)
    switch_int_types(type)
#undef switch_action
}

namespace {
template <typename T> inline int compare(T a, T b) { return a < b ? -1 : (a > b ? 1 : 0); }
} // namespace

bool typed_equal(Type type, uint64_t a, uint64_t b) {
#define switch_action(sw_typ) return ((sw_typ)a) == ((sw_typ)b);
    switch_int_types(type)
#undef switch_action
}

int typed_compare(Type type, uint64_t a, uint64_t b, bool signed_compare) {
#define switch_action(sw_typ) return signed_compare ? compare((SignedIntT<sw_typ>)a, (SignedIntT<sw_typ>)b) : compare(a, b);
    switch_int_types(type)
#undef switch_action
}

uint64_t typed_narrow(Type type, uint64_t value) {
#define switch_action(sw_typ) return (uint64_t)(sw_typ)value;
    switch_int_types(type)
#undef switch_action
}

// Check that the compiler handles signed shift as arithmetic (and not logical) shift.
static_assert((-1 >> 1) == -1, "This compiler treats signed shifts as logical shifts");
