#include "ir/optimizer/dce.h"

namespace optimizer {

void dce(IR *ir) {
    for (auto &bb : ir->basic_blocks) {
        for (size_t i = bb->variables.size(); i > 0; i--) {
            const auto &var = bb->variables[i - 1];
            if (var->ref_count > 0)
                continue;
            if (var->is_static()) // TODO
                continue;
            if (var->is_operation() && var->get_operation().type == Instruction::store)
                continue;
            bb->variables.erase(std::next(bb->variables.begin(), i - 1));
        }
    }
}

} // namespace optimizer
