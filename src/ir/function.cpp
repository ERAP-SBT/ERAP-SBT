#include "ir/function.h"

void Function::print(std::ostream &stream, const IR *ir) const {
    // not sure how functions will look like later so just some *really* basic output
    stream << "function f" << id << "(TODO) {\n";

    for (const auto *block : blocks) {
        block->print_name(stream, ir);
        stream << '\n';
    }

    stream << "} (TODO);";
}

void Function::print_name(std::ostream &stream, const IR *) const { stream << "f" << id; }
