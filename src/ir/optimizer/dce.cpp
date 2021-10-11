#include "ir/optimizer/dce.h"

#include "ir/optimizer/common.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <queue>
#include <set>
#include <vector>

namespace optimizer {

namespace {
template <typename Container> void remove_indices_sorted(Container &container, const std::vector<size_t> &indices) {
    size_t prev_index = SIZE_MAX;

    for (auto it = indices.rbegin(), end = indices.rend(); it != end; ++it) {
        size_t index = *it;
        assert(index < prev_index && index < container.size());
        prev_index = index;

        container.erase(std::next(container.begin(), index));
    }
}

bool check_target_input_removable(const BasicBlock *block, const BasicBlock *target) {
    assert(block && target);

    bool found_cf = false;
    for (const auto &cf : block->control_flow_ops) {
        switch (cf.type) {
        case CFCInstruction::jump:
        case CFCInstruction::cjump:
        case CFCInstruction::call:
        case CFCInstruction::syscall:
            if (cf.target() == target)
                found_cf = true;
            break;
        case CFCInstruction::ijump: {
            const auto &info = std::get<CfOp::IJumpInfo>(cf.info);
            if (info.targets.size() > 1) {
                // Ignore ijumps with multiple targets for now
                return false;
            } else if (info.targets.size() == 1 && info.targets[0] == target) {
                found_cf = true;
            }
            break;
        }
        case CFCInstruction::icall: {
            const auto &info = std::get<CfOp::ICallInfo>(cf.info);
            if (info.targets.size() > 1) {
                // Ignore icalls with multiple targets for now
                return false;
            } else if (info.targets.size() == 1 && info.targets[0] == target) {
                found_cf = true;
            }
            break;
        }
        case CFCInstruction::_return:
            return false;
        case CFCInstruction::unreachable:
            break;
        }
    }
    return found_cf;
}

bool remove_target_inputs(BasicBlock *block, const BasicBlock *target, const std::vector<size_t> &indices) {
    assert(block && target);

    bool found_cf = false;
    for (auto &cf : block->control_flow_ops) {
        switch (cf.type) {
        case CFCInstruction::jump: {
            auto &info = std::get<CfOp::JumpInfo>(cf.info);
            if (info.target != target)
                continue;
            assert(info.target_inputs.size() == target->inputs.size());
            remove_indices_sorted(info.target_inputs, indices);
            break;
        }
        case CFCInstruction::cjump: {
            auto &info = std::get<CfOp::CJumpInfo>(cf.info);
            if (info.target != target)
                continue;
            assert(info.target_inputs.size() == target->inputs.size());
            remove_indices_sorted(info.target_inputs, indices);
            break;
        }
        case CFCInstruction::syscall: {
            auto &info = std::get<CfOp::SyscallInfo>(cf.info);
            if (info.continuation_block != target)
                continue;
            assert(info.continuation_mapping.size() == target->inputs.size());
            remove_indices_sorted(info.continuation_mapping, indices);
            break;
        }
        case CFCInstruction::ijump: {
            auto &info = std::get<CfOp::IJumpInfo>(cf.info);
            bool ijump_has_target = false;
            for (auto *ijump_target : info.targets) {
                if (ijump_target == target) {
                    ijump_has_target = true;
                    break;
                }
            }
            if (!ijump_has_target)
                continue;
            assert(info.mapping.size() == target->inputs.size());
            remove_indices_sorted(info.mapping, indices);
            break;
        }
        case CFCInstruction::call: {
            auto &info = std::get<CfOp::CallInfo>(cf.info);
            if (info.target != target)
                continue;
            assert(info.target_inputs.size() == target->inputs.size());
            remove_indices_sorted(info.target_inputs, indices);
            break;
        }
        case CFCInstruction::icall: {
            auto &info = std::get<CfOp::ICallInfo>(cf.info);
            if (std::find(info.targets.begin(), info.targets.end(), target) == info.targets.end())
                continue;
            assert(info.mapping.size() == target->inputs.size());
            remove_indices_sorted(info.mapping, indices);
            break;
        }
        case CFCInstruction::_return:
        case CFCInstruction::unreachable:
            continue;
        }

        found_cf = true;
    }

    return found_cf;
}

bool mark(SSAVar *var, std::vector<bool> &track_buf, std::queue<SSAVar *> *visit_queue) {
    bool mark_val = true;
    std::swap(track_buf[var->id], mark_val);
    if (!mark_val) {
        if (visit_queue)
            visit_queue->push(var);
        return true;
    }
    return false;
}

bool propagate_from_successor(const BasicBlock *successor, const std::vector<bool> &successor_marks, const BasicBlock *current, std::vector<bool> &current_marks, std::queue<SSAVar *> *visit_queue) {
    assert(successor && successor_marks.size() >= successor->inputs.size());
    assert(current && current_marks.size() >= current->inputs.size());

    bool has_changed = false;

    bool found_cf = false;
    for (auto &cf : current->control_flow_ops) {
        switch (cf.type) {
        case CFCInstruction::jump: {
            auto &info = std::get<CfOp::JumpInfo>(cf.info);
            if (info.target != successor)
                continue;
            assert(info.target_inputs.size() == successor->inputs.size());
            for (size_t i = 0; i < info.target_inputs.size(); i++) {
                if (successor_marks[successor->inputs[i]->id]) {
                    has_changed |= mark(info.target_inputs[i].get(), current_marks, visit_queue);
                }
            }
            break;
        }
        case CFCInstruction::cjump: {
            auto &info = std::get<CfOp::CJumpInfo>(cf.info);
            if (info.target != successor)
                continue;
            assert(info.target_inputs.size() == successor->inputs.size());
            for (size_t i = 0; i < info.target_inputs.size(); i++) {
                if (successor_marks[successor->inputs[i]->id]) {
                    has_changed |= mark(info.target_inputs[i].get(), current_marks, visit_queue);
                }
            }
            break;
        }
        case CFCInstruction::syscall: {
            auto &info = std::get<CfOp::SyscallInfo>(cf.info);
            if (info.continuation_block != successor)
                continue;
            assert(info.continuation_mapping.size() == successor->inputs.size());
            for (size_t i = 0; i < info.continuation_mapping.size(); i++) {
                if (successor_marks[successor->inputs[i]->id]) {
                    has_changed |= mark(info.continuation_mapping[i].first.get(), current_marks, visit_queue);
                }
            }
            break;
        }
        case CFCInstruction::ijump: {
            auto &info = std::get<CfOp::IJumpInfo>(cf.info);
            if (info.targets.size() != 1 || info.targets[0] != successor)
                continue;
            assert(info.mapping.size() == successor->inputs.size());
            for (size_t i = 0; i < info.mapping.size(); i++) {
                if (successor_marks[successor->inputs[i]->id]) {
                    has_changed |= mark(info.mapping[i].first.get(), current_marks, visit_queue);
                }
            }
            break;
        }
        case CFCInstruction::call: {
            auto &info = std::get<CfOp::CallInfo>(cf.info);
            if (info.continuation_block == successor) {
                found_cf = true;
                continue;
            }
            if (info.target != successor)
                continue;
            assert(info.target_inputs.size() == successor->inputs.size());
            for (size_t i = 0; i < info.target_inputs.size(); i++) {
                if (successor_marks[successor->inputs[i]->id]) {
                    has_changed |= mark(info.target_inputs[i].get(), current_marks, visit_queue);
                }
            }
            break;
        }
        case CFCInstruction::icall: {
            auto &info = std::get<CfOp::ICallInfo>(cf.info);
            if (info.continuation_block == successor) {
                found_cf = true;
                continue;
            }
            if (info.targets.size() != 1 || info.targets[0] != successor)
                continue;
            assert(info.mapping.size() == successor->inputs.size());
            for (size_t i = 0; i < info.mapping.size(); i++) {
                if (successor_marks[successor->inputs[i]->id]) {
                    has_changed |= mark(info.mapping[i].first.get(), current_marks, visit_queue);
                }
            }
            break;
        }
        case CFCInstruction::_return:
        case CFCInstruction::unreachable:
            continue;
        }

        found_cf = true;
    }
    assert(found_cf);

    return has_changed;
}

bool visit_cf_and_mark_side_effects(const BasicBlock *block, std::vector<bool> &track_buf, std::queue<SSAVar *> *visit_queue) {
    bool has_changed = false;
    for (auto &cf : block->control_flow_ops) {
        for (auto &var : cf.in_vars) {
            if (var) {
                has_changed |= mark(var.get(), track_buf, visit_queue);
            }
        }

        switch (cf.type) {
        case CFCInstruction::ijump: {
            auto &info = std::get<CfOp::IJumpInfo>(cf.info);
            for (auto &m : info.mapping) {
                has_changed |= mark(m.first.get(), track_buf, visit_queue);
            }
            break;
        }
        case CFCInstruction::icall: {
            auto &info = std::get<CfOp::ICallInfo>(cf.info);
            for (auto &m : info.mapping) {
                has_changed |= mark(m.first.get(), track_buf, visit_queue);
            }
            break;
        }
        case CFCInstruction::_return: {
            auto &info = std::get<CfOp::RetInfo>(cf.info);
            for (auto &m : info.mapping) {
                has_changed |= mark(m.first.get(), track_buf, visit_queue);
            }
            break;
        }
        default:
            break;
        }
    }
    return has_changed;
}

void fixup_block_predecessors(std::vector<BasicBlock *> &predecessors, size_t bb_id) {
    // TODO this should be fixed in the lifter
    std::set<BasicBlock *> preds_dedup;
    for (auto *pred : predecessors) {
        preds_dedup.insert(pred);
    }
    if (preds_dedup.size() != predecessors.size()) {
        std::cerr << "Warning: Duplicate predecessor in block b" << bb_id << '\n';
    }
}

} // namespace

void dce(IR *ir) {
    std::vector<std::vector<bool>> usage;
    usage.resize(ir->cur_block_id);

    std::set<BasicBlock *> pending_blocks;

    // Remove unused variables by ref-count and track side effects
    for (auto &bb : ir->basic_blocks) {
        auto &track_buf = usage[bb->id];
        track_buf.resize(bb->cur_ssa_id);

        visit_cf_and_mark_side_effects(bb.get(), track_buf, nullptr);

        for (size_t i = bb->variables.size(); i > 0; i--) {
            const auto *var = bb->variables[i - 1].get();

            if (var->is_operation() && (var->get_operation().type == Instruction::store || track_buf[var->id])) {
                for (auto &in : var->get_operation().in_vars) {
                    if (in) {
                        track_buf[in->id] = true;
                    }
                }
            }

            if (var->ref_count == 0) {
                if (var->is_static())
                    continue;
                if (var->is_operation() && var->get_operation().type == Instruction::store)
                    continue;

                bb->variables.erase(std::next(bb->variables.begin(), i - 1));
            }
        }

        fixup_block_predecessors(bb->predecessors, bb->id);

        std::vector<size_t> unused_indices;
        for (size_t i = 0; i < bb->inputs.size(); i++) {
            auto *input = bb->inputs[i];
            if (input->ref_count == 0) {
                unused_indices.push_back(i);
            }
        }

        bool queue_preds = false;
        if (!unused_indices.empty()) {
            bool can_remove = true;
            for (auto *pred : bb->predecessors) {
                if (!check_target_input_removable(pred, bb.get())) {
                    can_remove = false;
                    break;
                }
            }

            if (can_remove) {
                for (auto *pred : bb->predecessors) {
                    if (!remove_target_inputs(pred, bb.get(), unused_indices)) {
                        panic("Target input removal failed, even though it shouldn't");
                    }
                }

                remove_indices_sorted(bb->inputs, unused_indices);

                queue_preds = true;
            }
        }

        if (!queue_preds) {
            for (size_t i = 0; i < bb->inputs.size(); i++) {
                if (track_buf[bb->inputs[i]->id]) {
                    queue_preds = true;
                    break;
                }
            }
        }

        if (queue_preds || true) {
            for (auto *pred : bb->predecessors) {
                pending_blocks.insert(pred);
            }
        }
    }

    // Propagate side effect tracking through basic blocks
    while (!pending_blocks.empty()) {
        auto *block = *pending_blocks.begin();
        pending_blocks.erase(pending_blocks.begin());

        auto &side_effects = usage[block->id];

        std::queue<SSAVar *> var_visit;

        for (const auto *succ : block->successors) {
            propagate_from_successor(succ, usage[succ->id], block, side_effects, &var_visit);
        }

        bool queue_preds = false;

        while (!var_visit.empty()) {
            const auto *var = var_visit.front();
            var_visit.pop();

            // `store`s have already been marked
            if (var->is_operation()) {
                if (!side_effects[var->id])
                    continue;

                for (auto &in : var->get_operation().in_vars) {
                    if (!in)
                        continue;

                    mark(in.get(), side_effects, &var_visit);
                }
            } else if (var->is_static()) {
                queue_preds = true;
            }
        }

        if (queue_preds) {
            for (auto *pred : block->predecessors) {
                pending_blocks.insert(pred);
            }
        }
    }

    // Remove all parameters which do not participate in any side effect
    for (auto &block : ir->basic_blocks) {
        auto &side_effects = usage[block->id];

        std::vector<size_t> unused_indices;
        for (size_t i = 0; i < block->inputs.size(); i++) {
            auto *input = block->inputs[i];
            if (input->ref_count == 0) {
                assert(!side_effects[input->id]);
                unused_indices.push_back(i);
            } else if (!side_effects[input->id]) {
                unused_indices.push_back(i);
            }
        }

        if (!unused_indices.empty()) {
            bool can_remove = true;

            for (auto *pred : block->predecessors) {
                if (!check_target_input_removable(pred, block.get())) {
                    can_remove = false;
                    break;
                }
            }

            if (can_remove) {
                for (auto *pred : block->predecessors) {
                    if (!remove_target_inputs(pred, block.get(), unused_indices)) {
                        panic("Could not find cfop with this block as target");
                    }
                }

                remove_indices_sorted(block->inputs, unused_indices);
            }
        }
    }

    // Do another ref-count removal
    for (auto &block : ir->basic_blocks) {
        for (size_t i = block->variables.size(); i > 0; i--) {
            const auto *var = block->variables[i - 1].get();

            if (var->ref_count == 0) {
                if (var->is_operation() && var->get_operation().type == Instruction::store)
                    continue;

                assert(!usage[block->id][var->id]);

                // if a static has a ref-count of 0, it should not be an input anymore by now.

                block->variables.erase(std::next(block->variables.begin(), i - 1));
            }
        }
    }
}

} // namespace optimizer
