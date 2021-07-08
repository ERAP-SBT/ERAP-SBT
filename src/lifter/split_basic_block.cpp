#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::split_basic_block(BasicBlock *bb, uint64_t addr, ELF64File *elf_base) const {
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
            // always fill the mapping if it is empty
            if (mapping.at(lifterInfo.static_id) == nullptr) {
                mapping.at(lifterInfo.static_id) = *it;
            } else if (!std::holds_alternative<size_t>(mapping.at(lifterInfo.static_id)->info)) {
                // replace statics if other variables are available
                mapping.at(lifterInfo.static_id) = *it;
            }
        }
    }
    mapping.at(ZERO_IDX) = nullptr;

    // create the new BasicBlock
    BasicBlock *new_bb = ir->add_basic_block(addr, elf_base ? elf_base->symbol_str_at_addr(addr).value_or("") : "");

    // transfer the control flow operations
    new_bb->control_flow_ops.swap(bb->control_flow_ops);
    bb->control_flow_ops.clear();

    // transfer and add predecessors and successors
    new_bb->predecessors.push_back(bb);
    new_bb->successors.swap(bb->successors);
    bb->successors.clear();
    bb->successors.push_back(new_bb);

    // correct the start and end addresses
    new_bb->set_virt_end_addr(bb->virt_end_addr);
    bb->set_virt_end_addr(std::get<SSAVar::LifterInfo>(first_bb_vars.front()->lifter_info).assign_addr);

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
            if (i != ZERO_IDX) {
                new_mapping.at(i) = new_bb->add_var_from_static(i, addr);
            } else {
                mapping.at(i) = nullptr;
            }
        }
    }

    // store the variables in the new basic block and adjust their inputs (if necessary)
    for (auto it = second_bb_vars.rbegin(); it != second_bb_vars.rend(); it++) {
        // add variable to the new BasicBlock
        new_bb->variables.push_back(std::move(*it));
        SSAVar *var = new_bb->variables.back().get();

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
                    in_var = new_mapping.at(in_var_lifter_info.static_id);
                }
            }
        }
    }

    // adjust the inputs of the cfop (if necessary)
    for (size_t i = 0; i < new_bb->control_flow_ops.size(); ++i) {
        auto &cf_op = new_bb->control_flow_ops.at(i);
        cf_op.source = new_bb;

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

        cf_op.clear_target_inputs();
        for (size_t j = 0; j < new_mapping.size(); j++) {
            auto var = new_mapping.at(j);
            if (var != nullptr) {
                cf_op.add_target_input(var, j);
                std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = j;
            }
        }
    }
}
