#include <lifter/lifter.h>

using namespace lifter::RV64;

std::vector<RefPtr<SSAVar>> Lifter::filter_target_inputs(const std::vector<RefPtr<SSAVar>> &old_target_inputs, reg_map new_mapping, uint64_t split_addr) const {
    std::vector<RefPtr<SSAVar>> new_target_inputs;
    for (const auto &ref_target_input : old_target_inputs) {
        if (ref_target_input->lifter_info.index() == 1) {
            auto &lifterInfo = std::get<SSAVar::LifterInfo>(ref_target_input->lifter_info);

            // only adjust target_inputs if they are in the first BasicBlock
            if (lifterInfo.assign_addr < split_addr) {
                new_target_inputs.emplace_back(RefPtr<SSAVar>(new_mapping.at(lifterInfo.static_id)));
            } else {
                new_target_inputs.emplace_back(ref_target_input);
            }
        }
    }
    return new_target_inputs;
}

std::vector<std::pair<RefPtr<SSAVar>, size_t>> Lifter::filter_target_inputs(const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &old_target_inputs, reg_map new_mapping, uint64_t split_addr) const {
    std::vector<std::pair<RefPtr<SSAVar>, size_t>> new_target_inputs;
    for (const auto &ref_target_input : old_target_inputs) {
        if (ref_target_input.first->lifter_info.index() == 1) {
            auto &lifterInfo = std::get<SSAVar::LifterInfo>(ref_target_input.first->lifter_info);

            // only adjust target_inputs if they are in the first BasicBlock
            if (lifterInfo.assign_addr < split_addr) {
                new_target_inputs.emplace_back(RefPtr<SSAVar>(new_mapping.at(ref_target_input.second)), ref_target_input.second);
            } else {
                new_target_inputs.emplace_back(ref_target_input.first, ref_target_input.second);
            }
        }
    }
    return new_target_inputs;
}

void Lifter::split_basic_block(BasicBlock *bb, uint64_t addr) const {
    // divide SSAVars in two to categories, the one before the address and the one at or after the address
    std::vector<SSAVar *> first_bb_vars{};
    std::vector<std::unique_ptr<SSAVar>> second_bb_vars{};

    // Additionally, reset all static mappings where variables were mapped to a static mapper (not received from one)
    for (int i = (int)bb->variables.size() - 1; i >= 0; --i) {
        auto &var = bb->variables.at(i);
        if (var->lifter_info.index() == 1) {
            auto &lifterInfo = std::get<SSAVar::LifterInfo>(var->lifter_info);
            if (lifterInfo.assign_addr < addr) {
                first_bb_vars.push_back(var.get());
            } else {
                second_bb_vars.push_back(std::move(bb->variables.at(i)));
                bb->variables.erase(std::next(bb->variables.begin(), i));
            }
        }
    }

    // recreate the register mapping at the given address
    reg_map mapping{};
    for (auto it = first_bb_vars.rbegin(); it != first_bb_vars.rend(); ++it) {
        if ((*it)->lifter_info.index() == 1) {
            auto &lifterInfo = std::get<SSAVar::LifterInfo>((*it)->lifter_info);
            if (!std::holds_alternative<size_t>((*it)->info) && mapping.at(lifterInfo.static_id) == nullptr) {
                mapping.at(lifterInfo.static_id) = *it;
            }
        }
    }
    // create the new BasicBlock
    BasicBlock *new_bb = ir->add_basic_block(addr);

    // transfer the control flow operations
    new_bb->control_flow_ops = std::move(bb->control_flow_ops);

    // transfer and add predecessors and successors
    new_bb->predecessors.push_back(bb);
    new_bb->successors = std::move(bb->successors);
    bb->successors.push_back(new_bb);

    // correct the start and end addresses
    new_bb->virt_end_addr = bb->virt_end_addr;
    bb->virt_end_addr = std::get<SSAVar::LifterInfo>(first_bb_vars.front()->lifter_info).assign_addr;

    // the register mapping in the BasicBlock
    reg_map new_mapping{};
    {
        // add a jump from the first to the second BasicBlock
        auto &cf_op = bb->add_cf_op(CFCInstruction::jump, new_bb, bb->virt_end_addr, addr);

        // static assignments
        for (size_t i = 0; i < mapping.size(); i++) {
            if (mapping.at(i) != nullptr) {
                cf_op.add_target_input(mapping.at(i), i);
                std::get<SSAVar::LifterInfo>(mapping.at(i)->lifter_info).static_id = i;
            }
            new_mapping.at(i) = new_bb->add_var_from_static(i, addr);
        }
    }

    // store the variables in the new basic block and adjust their inputs (if necessary)
    for (auto it = second_bb_vars.rbegin(); it != second_bb_vars.rend(); it++) {
        SSAVar *var = it->get();

        // add variable to the new BasicBlock
        new_bb->variables.push_back(std::move(*it));

        // set new id according to the new BasicBlocks ids
        var->id = new_bb->cur_ssa_id++;

        // variable is the result of an Operation -> adjust the inputs of the operation
        if (std::holds_alternative<std::unique_ptr<Operation>>(var->info)) {
            auto *operation = std::get<std::unique_ptr<Operation>>(var->info).get();
            for (auto &in_var : operation->in_vars) {
                // skip nullptrs
                if (in_var == nullptr) {
                    continue;
                }

                auto &in_var_lifter_info = std::get<SSAVar::LifterInfo>(in_var->lifter_info);

                // the input must only be changed if the input variable is in the first BasicBlock
                if (in_var_lifter_info.assign_addr < addr) {
                    in_var = mapping.at(in_var_lifter_info.static_id);
                }
            }
        }
    }

    // adjust the inputs of the cfop (if necessary)
    for (size_t i = 0; i < new_bb->control_flow_ops.size(); ++i) {
        auto &cf_op = new_bb->control_flow_ops.at(i);
        BasicBlock *target;
        target = cf_op.target();
        if (target) {
            // remove first BasicBlock from predecessors of the target of the Control-Flow-Operation
            auto it = std::find(target->predecessors.begin(), target->predecessors.end(), bb);
            if (it != target->predecessors.end()) {
                target->predecessors.erase(it);
            }

            // add new BasicBlock to predecessors of the target
            target->predecessors.push_back(new_bb);
        }

        switch (cf_op.type) {
        case CFCInstruction::jump:
            std::get<CfOp::JumpInfo>(cf_op.info).target_inputs = filter_target_inputs(std::get<CfOp::JumpInfo>(cf_op.info).target_inputs, new_mapping, addr);
            break;
        case CFCInstruction::cjump:
            std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs = filter_target_inputs(std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs, new_mapping, addr);
            break;
        case CFCInstruction::call:
            std::get<CfOp::CallInfo>(cf_op.info).target_inputs = filter_target_inputs(std::get<CfOp::CallInfo>(cf_op.info).target_inputs, new_mapping, addr);
            break;
        case CFCInstruction::ijump:
            std::get<CfOp::IJumpInfo>(cf_op.info).mapping = filter_target_inputs(std::get<CfOp::IJumpInfo>(cf_op.info).mapping, new_mapping, addr);
            break;
        case CFCInstruction::syscall:
            std::get<CfOp::SyscallInfo>(cf_op.info).continuation_mapping = filter_target_inputs(std::get<CfOp::SyscallInfo>(cf_op.info).continuation_mapping, new_mapping, addr);
            break;
        default:
            assert(0);
        }
    }
}
