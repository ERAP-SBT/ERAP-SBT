#include "ir/ir.h"

#include "ir/variable.h"

#include <sstream>
#include <unordered_set>

bool IR::verify(std::vector<std::string> &messages_out) const {
    bool ok = true;

    for (const auto &bb : basic_blocks) {
        if (!bb->verify(messages_out)) {
            ok = false;
        }
    }

    return ok;
}

void IR::print(std::ostream &stream) const {
    stream << "// GP-IR\n";
    for (const auto &static_var : statics)
        static_var.print(stream);

    for (const auto &basic_block : basic_blocks) {
        stream << '\n';
        basic_block->print(stream, this);
        stream << '\n';
    }
    stream << "\n";
}
