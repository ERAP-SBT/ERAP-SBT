#pragma once

#include "type.h"
#include <memory>
#include <string>

// forward declaration
class Operation;

class Variable
{
    private:
    // unique ssa-variable id, counting from 0 (holes might occur)
    const size_t id;
    const Type type;
    const int64_t immediate = 0;

    // for backtracking purposes, note the operation which assigns this variable
    std::shared_ptr<Operation> operation;
    size_t static_idx = 0;
    bool assigned_from_static = false;

    public:
    Variable(size_t id, Type type) : id(id), type(type) { }
    Variable(size_t id, int64_t imm) : id(id), type(Type::imm), immediate(imm), operation(nullptr) { }
    Variable(size_t id, Type type, size_t static_idx) : id(id), type(type), static_idx(static_idx), assigned_from_static(true) { }

    void set_op(std::shared_ptr<Operation> operation)
    {
      this->operation = operation;
    }
	
    // Getters
    size_t get_id() const { return id; }
    Type get_type() const { return type; }
    std::shared_ptr<Operation> get_operation() const { return operation; }
    int64_t get_immediate() const { return immediate; }

    bool is_assigned_from_static() const { return assigned_from_static; }
    size_t get_static_idx() const { return static_idx; }

    void print_name_type(std::ostream&) const;
    void print(std::ostream&) const;
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
    StaticMapper(std::string name, Type type) : name(name), type(type) {}

    // Getters
    const std::string &get_name() const { return name; }
    Type get_type() const { return type; }

    void print(std::ostream&) const;
};