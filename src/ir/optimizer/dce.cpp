#include "ir/optimizer/dce.h"

#include "ir/optimizer/common.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <set>
#include <vector>

namespace optimizer {

template <typename Container> static void remove_indices_sorted(Container &container, const std::vector<size_t> &indices) {
    for (auto it = indices.rbegin(), end = indices.rend(); it != end; ++it) {
        size_t index = *it;
        assert(index < container.size());
        container.erase(std::next(container.begin(), index));
    }
}

void dce(IR *ir) {
    bool cont;
    do {
        cont = false;
        for (auto &bb : ir->basic_blocks) {
            for (size_t i = bb->variables.size(); i > 0; i--) {
                const auto *var = bb->variables[i - 1].get();
                if (var->ref_count > 0)
                    continue;
                if (var->is_static())
                    continue;
                if (var->is_operation() && var->get_operation().type == Instruction::store)
                    continue;
                bb->variables.erase(std::next(bb->variables.begin(), i - 1));
            }

            std::vector<size_t> unused_indices;
            for (size_t i = 0; i < bb->inputs.size(); i++) {
                auto *input = bb->inputs[i];
                if (input->ref_count == 0) {
                    unused_indices.push_back(i);
                }
            }

            cont |= !unused_indices.empty();

            // TODO something generates duplicate predecessors
            std::set<BasicBlock *> preds_dedup;
            for (auto *pred : bb->predecessors) {
                preds_dedup.insert(pred);
            }
            if (preds_dedup.size() != bb->predecessors.size()) {
                std::cerr << "Warning: Duplicate predecessor in block b" << bb->id << '\n';
            }

            for (auto *pred : preds_dedup) {
                bool found_cf = false;
                for (auto &cf : pred->control_flow_ops) {
                    if (cf.continuation_target() != bb.get())
                        continue;
                    found_cf = true;
                    if (cf.type == CFCInstruction::jump) {
                        auto &info = std::get<CfOp::JumpInfo>(cf.info);
                        assert(info.target_inputs.size() == bb->inputs.size());
                        remove_indices_sorted(info.target_inputs, unused_indices);
                    } else if (cf.type == CFCInstruction::cjump) {
                        auto &info = std::get<CfOp::CJumpInfo>(cf.info);
                        assert(info.target_inputs.size() == bb->inputs.size());
                        remove_indices_sorted(info.target_inputs, unused_indices);
                    } else if (cf.type == CFCInstruction::call) {
                        auto &info = std::get<CfOp::CallInfo>(cf.info);
                        assert(info.target_inputs.size() == bb->inputs.size());
                        remove_indices_sorted(info.continuation_mapping, unused_indices);
                    } else if (cf.type == CFCInstruction::syscall) {
                        auto &info = std::get<CfOp::SyscallInfo>(cf.info);
                        assert(info.continuation_mapping.size() == bb->inputs.size());
                        remove_indices_sorted(info.continuation_mapping, unused_indices);
                    } else if (cf.type == CFCInstruction::ijump) {
                        // TODO
                        auto &info = std::get<CfOp::IJumpInfo>(cf.info);
                        assert(info.mapping.size() == bb->inputs.size());
                        remove_indices_sorted(info.mapping, unused_indices);
                    } else {
                        assert(false);
                    }
                }
                assert(found_cf);
            }

            remove_indices_sorted(bb->inputs, unused_indices);
        }
    } while (cont);
}

} // namespace optimizer
