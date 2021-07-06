#pragma once

#include "ref.h"
#include "type.h"

#include <memory>
#include <string>
#include <variant>

// forward declaration
struct Operation;
struct IR;

struct SSAVar : Refable {

    struct ImmInfo {
        int64_t val;

        /* true: val is relative to ir->base_addr
         * false: val is absolute
         */
        bool binary_relative = false;
    };

    struct LifterInfo {
        // the address at which the variable was assigned a value, where 0 := "unknown".
        uint64_t assign_addr = 0;
        // if the variable is `from_static`, this variable holds its actual index. Otherwise, this variable stores the potential static mapper index / register number.
        // The lifter uses this information mainly for basic block splitting and retro-fitting jumps into parsed basic blocks.
        size_t static_id = SIZE_MAX;
    };

    size_t id;
    Type type;
    bool const_evaluable = false;
    // immediate, static idx, op
    std::variant<std::monostate, ImmInfo, size_t, std::unique_ptr<Operation>> info;

    // Lifter-specific information which can be cleared afterwards
    std::variant<std::monostate, LifterInfo> lifter_info;

    SSAVar(const size_t id, const Type type) : id(id), type(type), info(std::monostate{}) {}
    SSAVar(const size_t id, const Type type, const size_t static_idx) : id(id), type(type), info(static_idx), lifter_info(LifterInfo{0, static_idx}) {}
    SSAVar(const size_t id, const int64_t imm, const bool binary_relative = false) : id(id), type(Type::imm), const_evaluable(true), info(ImmInfo{imm, binary_relative}) {}

    void set_op(std::unique_ptr<Operation> &&ptr);

    void print(std::ostream &, const IR *) const;
    void print_type_name(std::ostream &, const IR *) const;
};

/*
 * A static mapper is not a variable and thus not an ssa-variable.
 */
struct StaticMapper {
    const size_t id;
    const Type type;

    StaticMapper(size_t id, Type type) : id(id), type(type) {}

    void print(std::ostream &) const;
};
