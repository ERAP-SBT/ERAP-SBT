#include "ir/ir.h"

void IR::print(std::ostream &stream) const
{
    stream << "// GP-IR v" << IR_VERSION << "\n";
    for (const auto &static_var : statics)
        static_var.print(stream);

    for (const auto &basic_block : basic_blocks)
    {
        stream << '\n';
        basic_block->print(stream, this);
        stream << '\n';
    }
    stream << "\n";
}
