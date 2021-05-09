#include "../../include/ir/variable.h"

Variable::Variable(int id, Type type, std::shared_ptr<Operation> operation) : id(id), type(type), operation(std::move(operation)) { }

Variable::~Variable() = default;
