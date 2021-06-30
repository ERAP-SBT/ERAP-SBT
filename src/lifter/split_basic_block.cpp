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
            if (!std::holds_alternative<size_t>((*it)->info) && mapping.at(lifterInfo.static_id) == nullptr) {
                mapping.at(lifterInfo.static_id) = *it;
            }
        }
    }
    // create the new BasicBlock
    BasicBlock *new_bb = ir->add_basic_block(addr, elf_base->symbol_str_at_addr(addr).value_or(""));

    // transfer the control flow operations
    new_bb->control_flow_ops = std::move(bb->control_flow_ops);

    // transfer and add predecessors and successors
    new_bb->predecessors.push_back(bb);
    new_bb->successors = std::move(bb->successors);
    bb->successors.push_back(new_bb);

    // correct the start and end addresses
    std::get<1>(new_bb->lifter_info).second = std::get<1>(bb->lifter_info).second;
    std::get<1>(bb->lifter_info).second = std::get<SSAVar::LifterInfo>(first_bb_vars.front()->lifter_info).assign_addr;

    // the register mapping in the BasicBlock
    reg_map new_mapping{};
    {
        // add a jump from the first to the second BasicBlock
        auto &cf_op = bb->add_cf_op(CFCInstruction::jump, new_bb, std::get<1>(bb->lifter_info).second, addr);

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

        // add variable to the new BasicBlock -> this moving causes invalidated all references on the old ssa-var
        new_bb->variables.push_back(std::move(*it));
        for (auto &cf_op : new_bb->control_flow_ops) {
            for (auto &ref_ptr : cf_op.in_vars) {
                ref_ptr.reset(new_bb->variables.back().get());
            }
        }

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
