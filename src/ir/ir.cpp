#include "../../include/ir/ir.h"

IR::IR() : functions() { }
IR::~IR() = default;
void IR::add_function(const std::shared_ptr<Function> &new_function)
{
    functions.push_back(new_function);
}
