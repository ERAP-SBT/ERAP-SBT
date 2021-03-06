#include <lifter/lifter.h>

using namespace lifter::RV64;

BasicBlock *Lifter::split_basic_block(BasicBlock *bb, uint64_t addr, ELF64File *elf_base) const {
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

        // skip variables which aren't assigned to a register
        if (static_id == SIZE_MAX) {
            continue;
        }

        assert(static_id != 0 && static_id < count_used_static_vars);

        if (mapping[static_id] == nullptr && static_id != ZERO_IDX) {
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

    // transfer successors
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
                auto &info = std::get<CfOp::CallInfo>(cf_op.info);
                if (info.continuation_block->id == bb->id) {
                    old_target = bb;
                    info.continuation_block = new_bb;
                } else {
                    old_target = info.target;
                    info.target = new_bb;
                }
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
                    old_target->predecessors.erase(std::remove_if(old_target->predecessors.begin(), old_target->predecessors.end(), [block](const auto *b) { return block == b; }),
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

        if (jmp_addr >= bb->virt_start_addr && jmp_addr <= bb->virt_end_addr) {
            // the old block jumped to itself so we need to make the new bb a predecessor of it
            if (auto it = std::find(bb->predecessors.begin(), bb->predecessors.end(), bb); it != bb->predecessors.end()) {
                bb->predecessors.erase(it);
                bb->predecessors.emplace_back(new_bb);
            }
            continue;
        }

        if (jmp_addr < new_virt_start_addr || jmp_addr > new_virt_end_addr) {
            continue;
        }

        // should be bb
        auto *old_target = cf_op.target();
        cf_op.set_target(new_bb);
        auto target_still_present = false;
        for (const auto &cf_op : new_bb->control_flow_ops) {
            if (cf_op.target() == old_target) {
                target_still_present = true;
                break;
            }
        }
        if (!target_still_present) {
            new_bb->successors.erase(std::remove_if(new_bb->successors.begin(), new_bb->successors.end(), [old_target](const auto *b) { return old_target == b; }), new_bb->successors.end());

            if (old_target) {
                old_target->predecessors.erase(std::remove_if(old_target->predecessors.begin(), old_target->predecessors.end(), [old_target](const auto *b) { return old_target == b; }),
                                               old_target->predecessors.end());
            }
        }

        // make self-referencing
        if (std::find(new_bb->predecessors.begin(), new_bb->predecessors.end(), new_bb) == new_bb->predecessors.end()) {
            new_bb->predecessors.emplace_back(new_bb);
            // successor add should be able to go here
            // TODO: assert this
        }
        if (std::find(new_bb->successors.begin(), new_bb->successors.end(), new_bb) == new_bb->successors.end()) {
            new_bb->successors.emplace_back(new_bb);
        }
    }

    // the register mapping in the BasicBlock
    reg_map new_mapping{};
    reg_map static_inputs{};
    {
        // add a jump from the first to the second BasicBlock
        auto &cf_op = bb->add_cf_op(CFCInstruction::jump, new_bb, bb->virt_end_addr, addr);
        if (std::find(new_bb->predecessors.begin(), new_bb->predecessors.end(), bb) == new_bb->predecessors.end()) {
            // TODO: is this if necessary? need to check if the code above can push bb into new_bb->predecessors
            new_bb->predecessors.emplace_back(bb);
        }

        // static assignments
        for (size_t i = 0; i < count_used_static_vars; i++) {
            if (mapping[i] != nullptr) {
                if (i != 0) {
                    cf_op.add_target_input(mapping[i], i);
                }
                assert(std::get<SSAVar::LifterInfo>(mapping[i]->lifter_info).static_id == i);
            }
            if (i != ZERO_IDX) {
                new_mapping[i] = new_bb->add_var_from_static(i, addr);
                static_inputs[i] = new_mapping[i];
            } else {
                new_mapping[i] = nullptr;
                static_inputs[i] = nullptr;
            }
        }
    }

    // store the variables in the new basic block and adjust their inputs (if necessary)
    for (auto it = second_bb_vars.rbegin(); it != second_bb_vars.rend(); it++) {
        SSAVar *var = (*it).get();

        // variable is the result of an Operation -> adjust the inputs of the operation
        if (auto *operation = var->maybe_get_operation()) {
            for (auto &in_var : operation->in_vars) {
                // skip nullptrs
                if (in_var == nullptr) {
                    continue;
                }

                auto &in_var_lifter_info = std::get<SSAVar::LifterInfo>(in_var->lifter_info);

                // the input must only be changed if the input variable is in the first BasicBlock
                if (in_var_lifter_info.assign_addr < addr) {
                    SSAVar *new_var;
                    if (std::holds_alternative<size_t>(in_var->info)) {
                        new_var = static_inputs[in_var_lifter_info.static_id];
                    } else {
                        new_var = new_mapping[in_var_lifter_info.static_id];
                    }
                    if ((in_var->type == Type::imm && operation->type != Instruction::cast && new_var->type != operation->lifter_info.in_op_size) || cast_dir(in_var->type, new_var->type) == 1) {
                        SSAVar *const casted_value = new_bb->add_var(operation->lifter_info.in_op_size, std::get<SSAVar::LifterInfo>(var->lifter_info).assign_addr);
                        auto op = std::make_unique<Operation>(Instruction::cast);
                        op->lifter_info.in_op_size = new_var->type;
                        op->set_inputs(new_var);
                        op->set_outputs(casted_value);
                        casted_value->set_op(std::move(op));
                        in_var = casted_value;
                    } else {
                        in_var = new_var;
                    }
                }
            }
        }

        // set new id according to the new BasicBlocks ids
        var->id = new_bb->cur_ssa_id++;

        // add variable to the new BasicBlock
        new_bb->variables.push_back(std::move(*it));

        const auto static_id = std::get<SSAVar::LifterInfo>(var->lifter_info).static_id;
        if (static_id != ZERO_IDX && static_id != SIZE_MAX) {
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

        BasicBlock *target = cf_op.target();
        if (target && target != bb && target != new_bb) {
            // we already fixed up self-references
            // remove first BasicBlock from predecessors of the target of the Control-Flow-Operation
            auto it = std::find(target->predecessors.begin(), target->predecessors.end(), bb);
            if (it != target->predecessors.end()) {
                target->predecessors.erase(it);
            }

            // add new BasicBlock to predecessors of the target
            target->predecessors.push_back(new_bb);
        }
        if (cf_op.type == CFCInstruction::call || cf_op.type == CFCInstruction::icall) {
            BasicBlock *cont_block = (cf_op.type == CFCInstruction::call ? std::get<CfOp::CallInfo>(cf_op.info).continuation_block : std::get<CfOp::ICallInfo>(cf_op.info).continuation_block);
            if (cont_block) {
                // remove first BasicBlock from predecessors of the target of the Control-Flow-Operation
                auto it = std::find(cont_block->predecessors.begin(), cont_block->predecessors.end(), bb);
                if (it != cont_block->predecessors.end()) {
                    cont_block->predecessors.erase(it);
                }

                // add new BasicBlock to predecessors of the target
                cont_block->predecessors.push_back(new_bb);
            }
        }

        cf_op.clear_target_inputs();
        for (size_t j = 0; j < count_used_static_vars; j++) {
            auto var = new_mapping[j];
            if (var != nullptr) {
                cf_op.add_target_input(var, j);
                std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = j;
            }
        }
    }

    return new_bb;
}
