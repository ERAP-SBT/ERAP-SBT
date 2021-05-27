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
    size_t id;
    Type type;
    bool from_static = false;
    bool const_evaluable = false;
    // immediate, static idx, op
    std::variant<std::monostate, int64_t, size_t, std::unique_ptr<Operation>> info;

    SSAVar(const size_t id, const Type type) : id(id), type(type), info(std::monostate{}) {}
    SSAVar(const size_t id, const Type type, const size_t static_idx) : id(id), type(type), from_static(true), info(static_idx) {}
    SSAVar(const size_t id, const int64_t imm) : id(id), type(Type::imm), const_evaluable(true), info(imm) {}

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
