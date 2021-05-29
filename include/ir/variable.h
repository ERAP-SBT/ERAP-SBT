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
        bool binary_relative = false;
    };

    size_t id;
    Type type;
    bool from_static = false;
    bool const_evaluable = false;

    // if the variable is `from_static`, this variable hold it's index. This is required to be independent of the `info` variant,
    // otherwise binding a variable to a static mapper would overwrite its operation or immediate info, rendering it useless.
    size_t static_idx = SIZE_MAX;
    // immediate, static idx, op
    std::variant<std::monostate, ImmInfo, size_t, std::unique_ptr<Operation>> info;

    SSAVar(const size_t id, const Type type) : id(id), type(type), info(std::monostate{}) {}
    SSAVar(const size_t id, const Type type, const size_t static_idx) : id(id), type(type), from_static(true), static_idx(static_idx) {}
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
