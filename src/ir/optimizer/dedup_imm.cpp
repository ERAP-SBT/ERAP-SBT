#include "ir/optimizer/dedup_imm.h"

#include "ir/optimizer/common.h"

#include <unordered_map>
#include <vector>

template <> struct std::hash<SSAVar::ImmInfo> {
    size_t operator()(const SSAVar::ImmInfo &key) const noexcept { return std::hash<uint64_t>()(key.val) ^ std::hash<bool>()(key.binary_relative); }
};

namespace optimizer {

void dedup_imm(IR *ir) {
    for (auto &bb : ir->basic_blocks) {
        VarRewriter rw;
        std::unordered_map<SSAVar::ImmInfo, SSAVar *> imms;
        std::vector<size_t> deduplicated_indices;

        for (size_t vi = 0; vi < bb->variables.size(); vi++) {
            auto *var = bb->variables[vi].get();
            if (var->is_operation()) {
                rw.apply_to(var->get_operation());
            } else if (var->is_immediate()) {
                auto &imm = var->get_immediate();

                auto it = imms.find(imm);
                if (it != imms.end()) {
                    rw.replace(var, it->second);
                    deduplicated_indices.push_back(vi);
                } else {
                    imms.emplace(imm, var);
                }
            }
        }

        rw.apply_to(bb->control_flow_ops);

        for (auto it = deduplicated_indices.rbegin(), end = deduplicated_indices.rend(); it != end; ++it) {
            bb->variables.erase(std::next(bb->variables.begin(), *it));
        }
    }
}

} // namespace optimizer
