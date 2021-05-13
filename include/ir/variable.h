#pragma once

#include "type.h"
#include <memory>
#include <string>
#include <variant>

// forward declaration
struct Operation;
struct IR;

struct SSAVar
{
    size_t id;
    size_t ref_count = 0;
    Type type;
    bool from_static    = false;
    bool const_evalable = false;
    // immediate, static idx, op
    std::variant<std::monostate, int64_t, size_t, std::unique_ptr<Operation>> info;

    SSAVar(const size_t id, const Type type) : id(id), type(type), info(std::monostate{}) { }
    SSAVar(const size_t id, const Type type, const size_t static_idx) : id(id), type(type), info(static_idx) { }
    SSAVar(const size_t id, const int64_t imm) : id(id), type(Type::imm), const_evalable(true), info(imm) { }

    void set_op(std::unique_ptr<Operation> &&ptr);

    void print(std::ostream &, const IR *) const;
    void print_type_name(std::ostream &, const IR *) const;
};

/*
 * A static mapper is not a variable and thus not an ssa-variable.
 */
class StaticMapper
{
    private:
    const std::string name;
    const Type type;

    public:
    StaticMapper(std::string name, Type type) : name(name), type(type) { }

    // Getters
    const std::string &get_name() const { return name; }
    Type get_type() const { return type; }

    void print(std::ostream &) const;
};