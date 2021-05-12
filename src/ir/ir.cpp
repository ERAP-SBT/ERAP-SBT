#include "../../include/ir/ir.h"

void IR::add_function(const std::shared_ptr<Function> &new_function)
{
    functions.push_back(new_function);
}

void IR::print(std::ostream &stream) const {
    stream << "// GP-IR v0.1\n";
    for (const auto& static_var : statics)
        static_var.print(stream);
    stream << "\n";

    for (const auto& basic_block : basic_blocks)
        basic_block->print(stream);
    stream << "\n";

    for (const auto& function : functions)
        function->print(stream);
}
