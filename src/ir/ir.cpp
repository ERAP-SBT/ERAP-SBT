#include "../../include/ir/ir.h"

IR::IR() : functions() { }
IR::~IR() = default;
void IR::add_function(const std::shared_ptr<Function> &new_function)
{
    functions.push_back(new_function);
}
void IR::add_statics(const std::shared_ptr<StaticMapper> &new_static_var) {
    statics.push_back(new_static_var);
}
