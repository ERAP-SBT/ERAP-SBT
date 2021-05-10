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
    const int id;
    const Type type;

    // for backtracking purposes, note the operation which assigns this variable
    const std::shared_ptr<Operation> operation;

    public:
    Variable(int id, Type type, std::shared_ptr<Operation> operation);

    ~Variable();

    // Getters
    inline int get_id() const { return id; }
    inline Type get_type() const { return type; }
    inline std::shared_ptr<Operation> get_operation() const { return operation; }
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
    StaticMapper(std::string name, Type type);
    ~StaticMapper();

    // Getters
    inline std::string get_name() { return name; }
    inline Type get_type() const { return type; }
};