#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::split_basic_block(BasicBlock *bb, uint64_t addr, ELF64File *elf_base) const {
    // divide SSAVars in two to categories, the one before the address and the one at or after the address
    std::vector<SSAVar *> first_bb_vars{};
    std::vector<std::unique_ptr<SSAVar>> second_bb_vars{};

    // Additionally, reset all static mappings where variables were mapped to a static mapper (not received from one)
    for (int i = (int)bb->variables.size() - 1; i >= 0; --i) {
        auto &var = bb->variables[i];
        if (var->lifter_info.index() == 1) {
            auto &lifterInfo = std::get<SSAVar::LifterInfo>(var->lifter_info);
            if (lifterInfo.assign_addr < addr) {
                first_bb_vars.push_back(var.get());
            } else {
                second_bb_vars.push_back(std::move(bb->variables[i]));
                bb->variables.erase(std::next(bb->variables.begin(), i));
            }
        }
    }

    // recreate the register mapping at the given address
    reg_map mapping{};
    for (auto *var : first_bb_vars) {
        if (var->lifter_info.index() != 1) {
            continue;
        }

        const auto static_id = std::get<SSAVar::LifterInfo>(var->lifter_info).static_id;
        if (mapping[static_id] == nullptr || std::holds_alternative<size_t>(mapping[static_id]->info)) {
            mapping[static_id] = var;
        }
    }
    // zero extend all f32 values in order to map them correctly to the f64 statics
    zero_extend_all_f32(bb, mapping, addr);

    mapping[ZERO_IDX] = nullptr;

    // create the new BasicBlock
    BasicBlock *new_bb = ir->add_basic_block(addr, elf_base ? elf_base->symbol_str_at_addr(addr).value_or("") : "");

    // transfer the control flow operations
    new_bb->control_flow_ops.swap(bb->control_flow_ops);
    bb->control_flow_ops.clear();

    // transfer and add predecessors and successors
    new_bb->successors.swap(bb->successors);
    bb->successors.clear();

    // correct the start and end addresses
    new_bb->set_virt_end_addr(bb->virt_end_addr);
    bb->set_virt_end_addr(std::get<SSAVar::LifterInfo>(first_bb_vars.front()->lifter_info).assign_addr);

    // fix jump references
    const auto new_virt_start_addr = new_bb->virt_start_addr;
    const auto new_virt_end_addr = new_bb->virt_end_addr;
    for (auto *block : bb->predecessors) {
        for (auto &cf_op : block->control_flow_ops) {
            if (cf_op.lifter_info.index() != 1) {
                continue;
            }

            const auto jmp_addr = std::get<CfOp::LifterInfo>(cf_op.lifter_info).jump_addr;
            if (jmp_addr == 0 || std::holds_alternative<CfOp::IJumpInfo>(cf_op.info) || std::holds_alternative<CfOp::ICallInfo>(cf_op.info) || std::holds_alternative<CfOp::RetInfo>(cf_op.info)) {
                continue;
            }

            if (jmp_addr < new_virt_start_addr || jmp_addr > new_virt_end_addr) {
                continue;
            }

            BasicBlock *old_target = nullptr; // this should always turn out to be bb
            if (std::holds_alternative<CfOp::JumpInfo>(cf_op.info)) {
                auto &info = std::get<CfOp::JumpInfo>(cf_op.info);
                old_target = info.target;
                info.target = new_bb;
            } else if (std::holds_alternative<CfOp::CJumpInfo>(cf_op.info)) {
                auto &info = std::get<CfOp::CJumpInfo>(cf_op.info);
                old_target = info.target;
                info.target = new_bb;
            } else if (std::holds_alternative<CfOp::CallInfo>(cf_op.info)) {
                // TODO: needs more sophisticated logic
                assert(0);
                continue;
            } else if (std::holds_alternative<CfOp::SyscallInfo>(cf_op.info)) {
                auto &info = std::get<CfOp::SyscallInfo>(cf_op.info);
                old_target = info.continuation_block;
                info.continuation_block = new_bb;
            }

            auto target_still_present = false;
            for (const auto &cf_op : block->control_flow_ops) {
                if (cf_op.target() == old_target) {
                    target_still_present = true;
                    break;
                }
            }
            if (!target_still_present) {
                block->successors.erase(std::remove_if(block->successors.begin(), block->successors.end(), [old_target](const auto *b) { return old_target == b; }), block->successors.end());

                if (old_target) {
                    old_target->predecessors.erase(std::remove_if(old_target->predecessors.begin(), old_target->predecessors.end(), [old_target](const auto *b) { return old_target == b; }),
                                                   old_target->predecessors.end());
                }
            }

            block->successors.emplace_back(new_bb);
            new_bb->predecessors.emplace_back(block);
        }
    }

    // there can be recursive jumps into the block as well which need to be fixed
    for (auto &cf_op : new_bb->control_flow_ops) {
        if (cf_op.lifter_info.index() != 1) {
            continue;
        }

        const auto jmp_addr = std::get<CfOp::LifterInfo>(cf_op.lifter_info).jump_addr;
        if (jmp_addr == 0 || std::holds_alternative<CfOp::IJumpInfo>(cf_op.info) || std::holds_alternative<CfOp::ICallInfo>(cf_op.info) || std::holds_alternative<CfOp::RetInfo>(cf_op.info)) {
            continue;
        }

        if (jmp_addr < new_virt_start_addr || jmp_addr > new_virt_end_addr) {
            continue;
        }

        cf_op.set_target(new_bb);
    }

    // the register mapping in the BasicBlock
    reg_map new_mapping{};
    {
        // add a jump from the first to the second BasicBlock
        auto &cf_op = bb->add_cf_op(CFCInstruction::jump, new_bb, bb->virt_end_addr, addr);

        // static assignments
        for (size_t i = 0; i < mapping.size(); i++) {
            if (mapping[i] != nullptr) {
                if (i != 0) {
                    cf_op.add_target_input(mapping[i], i);
                }
                std::get<SSAVar::LifterInfo>(mapping[i]->lifter_info).static_id = i;
            }
            if (i != ZERO_IDX) {
                new_mapping[i] = new_bb->add_var_from_static(i, addr);
            } else {
                new_mapping[i] = nullptr;
            }
        }
    }

    // store the variables in the new basic block and adjust their inputs (if necessary)
    for (auto it = second_bb_vars.rbegin(); it != second_bb_vars.rend(); it++) {
        SSAVar *var = (*it).get();
        // set new id according to the new BasicBlocks ids
        var->id = new_bb->cur_ssa_id++;

        // variable is the result of an Operation -> adjust the inputs of the operation
        if (std::holds_alternative<std::unique_ptr<Operation>>(var->info)) {
            auto *operation = std::get<std::unique_ptr<Operation>>(var->info).get();
            const bool is_single_precision_op = is_float(var->type);
            for (auto &in_var : operation->in_vars) {
                // skip nullptrs
                if (in_var == nullptr) {
                    continue;
                }

                auto &in_var_lifter_info = std::get<SSAVar::LifterInfo>(in_var->lifter_info);

                // the input must only be changed if the input variable is in the first BasicBlock
                if (in_var_lifter_info.assign_addr < addr) {
                    if (is_single_precision_op && in_var->type == Type::f32) {
                        // cast f64 static to f32 if necessary
                        SSAVar *casted_in_var = new_bb->add_var(Type::f32, addr);
                        auto op = std::make_unique<Operation>(Instruction::cast);
                        op->set_inputs(new_mapping[in_var_lifter_info.static_id]);
                        op->set_outputs(casted_in_var);
                        casted_in_var->set_op(std::move(op));
                        new_mapping[in_var_lifter_info.static_id] = casted_in_var;
                    }
                    in_var = new_mapping[in_var_lifter_info.static_id];
                }
            }
        }

        // add variable to the new BasicBlock
        new_bb->variables.push_back(std::move(*it));

        const auto static_id = std::get<SSAVar::LifterInfo>(var->lifter_info).static_id;
        if (static_id != ZERO_IDX) {
            new_mapping[static_id] = var;
        }
    }

    // adjust the inputs of the cfop (if necessary)
    for (size_t i = 0; i < new_bb->control_flow_ops.size(); ++i) {
        auto &cf_op = new_bb->control_flow_ops[i];
        cf_op.source = new_bb;

        for (auto &var : cf_op.in_vars) {
            if (!var) {
                continue;
            }

            const auto &lifter_info = std::get<SSAVar::LifterInfo>(var->lifter_info);
            if (lifter_info.assign_addr < addr) {
                // TODO: this if should be unnecessary
                var.reset(new_mapping[lifter_info.static_id]);
            }
        }

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
            auto var = new_mapping[j];
            if (var != nullptr) {
                cf_op.add_target_input(var, j);
                std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = j;
            }
        }
    }
}
