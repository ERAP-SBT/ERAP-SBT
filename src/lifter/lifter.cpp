#include <frvdec.h>
#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift(Program *prog) {
    assert(prog->elf_base->base_addr <= prog->elf_base->load_end_addr);
    ir->base_addr = prog->elf_base->base_addr;
    ir->load_size = prog->elf_base->load_end_addr - prog->elf_base->base_addr;
    ir->phdr_num = prog->elf_base->program_headers.size();
    ir->phdr_off = prog->elf_base->phdr_offset;
    ir->phdr_size = prog->elf_base->phdr_size;
    ir->p_entry_addr = prog->elf_base->header.e_entry;
    dummy = ir->add_basic_block(0, "Dummy Basic Block");

    if (prog->elf_base->section_headers.empty()) {
        // preload all instructions which are located "in" the program headers which are loaded, executable and readable
        for (auto &prog_hdr : prog->elf_base->program_headers) {
            if (prog_hdr.p_type == PT_LOAD && prog_hdr.p_flags & PF_X && prog_hdr.p_flags & PF_R) {
                prog->load_instrs(prog->elf_base->file_content.data() + prog_hdr.p_offset, prog_hdr.p_filesz, prog_hdr.p_vaddr);
            } else if (prog_hdr.p_type == PT_LOAD && prog_hdr.p_flags & PF_W && prog_hdr.p_flags & PF_R) {
                prog->load_instrs(prog->elf_base->file_content.data() + prog_hdr.p_offset, prog_hdr.p_filesz, prog_hdr.p_vaddr);
            }
        }
    } else {
        // parse all executable sections (only if section information is contained in the elf binary)
        for (auto &sh_hdr : prog->elf_base->section_headers) {
            if (sh_hdr.sh_type & SHT_PROGBITS && sh_hdr.sh_flags & SHF_ALLOC) {
                if (sh_hdr.sh_flags & SHF_EXECINSTR) {
                    prog->load_instrs(prog->elf_base->file_content.data() + sh_hdr.sh_offset, sh_hdr.sh_size, sh_hdr.sh_addr);
                } else {
                    prog->load_data(prog->elf_base->file_content.data() + sh_hdr.sh_offset, sh_hdr.sh_size, sh_hdr.sh_addr);
                }
            }
        }
    }

    ir->setup_bb_addr_vec(prog->addrs[0], prog->addrs.back());

    needs_bb_start.clear();
    needs_bb_start.resize(ir->virt_bb_ptrs.size());
    needs_bb_start[(prog->elf_base->header.e_entry - ir->virt_bb_start_addr)] = true;

    add_statics();

    if (interpreter_only) {
        return;
    }

    BasicBlock *cur_bb = nullptr;
    reg_map mapping;
    const auto create_new_bb = [this, prog, &cur_bb, &mapping](uint64_t prev_addr, uint64_t virt_addr) {
        auto *new_bb = ir->add_basic_block(virt_addr, prog->elf_base->symbol_str_at_addr(virt_addr).value_or(""));

        if (cur_bb) {
            // create jump to new bb
            auto &cf_op = cur_bb->add_cf_op(CFCInstruction::jump, new_bb, prev_addr, virt_addr);
            std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.reserve(count_used_static_vars);
            for (size_t i = 0; i < count_used_static_vars; i++) {
                auto var = mapping[i];
                if (var != nullptr) {
                    cf_op.add_target_input(var, i);
                    std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = i;
                }
            }
            assert(cf_op.target_inputs().size() == count_used_static_vars - 1);
            cur_bb->set_virt_end_addr(prev_addr);
            cur_bb->variables.shrink_to_fit();
        }

        // load ssa variables from static vars and fill mapping
        for (size_t i = 0; i < count_used_static_vars; i++) {
            if (i != ZERO_IDX) {
                mapping[i] = new_bb->add_var_from_static(i, virt_addr);
            } else {
                mapping[i] = nullptr;
            }
        }

        cur_bb = new_bb;
    };

    for (size_t i = 0; i < prog->data.size(); ++i) {
        const auto virt_addr = prog->addrs[i];
        if (!std::holds_alternative<RV64Inst>(prog->data[i])) {
            if (cur_bb) {
                cur_bb->add_cf_op(CFCInstruction::unreachable, nullptr, virt_addr);
                cur_bb->variables.shrink_to_fit();
                cur_bb = nullptr;
            }
            continue;
        }

        // we scan top to bottom
        assert(ir->bb_at_addr(virt_addr) == nullptr);

        if (cur_bb && needs_bb_start[(virt_addr - ir->virt_bb_start_addr) / 2]) {
            create_new_bb(prog->addrs[i - 1], virt_addr);
        }

        const auto &instr = std::get<RV64Inst>(prog->data[i]);
        if (instr.instr.mnem == FRV_INVALID) {
            if (cur_bb) {
                cur_bb->add_cf_op(CFCInstruction::unreachable, nullptr, virt_addr);
                cur_bb->variables.shrink_to_fit();
                cur_bb = nullptr;
            }
            continue;
        }

        if (!cur_bb) {
            create_new_bb(0, virt_addr);
        }

        uint64_t next_addr;
        // TODO: this will only get hit at the very last addr so maybe just make prog->addrs one bigger and add a zero or smth
        if (i < prog->addrs.size() - 1) {
            next_addr = prog->addrs[i + 1];
        } else {
            next_addr = prog->addrs[i] + 2;
        }
        parse_instruction(cur_bb, instr, mapping, virt_addr, next_addr);
        if (cur_bb->control_flow_ops.empty()) {
            continue;
        }

        // we reached the end of a bblock
        cur_bb->set_virt_end_addr(virt_addr);
        // check if we need to split bblocks and mark needed bbs
        for (size_t i = 0; i < cur_bb->control_flow_ops.size(); ++i) {
            auto &cf_op = cur_bb->control_flow_ops[i];
            if (cf_op.type == CFCInstruction::unreachable) {
                continue;
            }

            if (cf_op.type == CFCInstruction::_return) {
                for (size_t i = 0; i < count_used_static_vars; i++) {
                    auto var = mapping[i];
                    if (var != nullptr) {
                        cf_op.add_target_input(var, i);
                        std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = i;
                    }
                }
                continue;
            }

            BasicBlock *next_bb;
            uint64_t jmp_addr = std::get<CfOp::LifterInfo>(cf_op.lifter_info).jump_addr;
            if (is_jump_table_jump(cur_bb, cf_op, instr, prog) || jmp_addr == 0) {
                next_bb = dummy;
            } else {
                next_bb = get_bb(jmp_addr);
            }

            // when we split a bblock the target_inputs get filled by split_basic_block
            if (cf_op.target_inputs().empty()) {
                switch (cf_op.type) {
                case CFCInstruction::jump:
                    std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.reserve(count_used_static_vars);
                    break;
                case CFCInstruction::ijump:
                    std::get<CfOp::IJumpInfo>(cf_op.info).mapping.reserve(count_used_static_vars);
                    break;
                case CFCInstruction::cjump:
                    std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs.reserve(count_used_static_vars);
                    break;
                case CFCInstruction::call:
                    std::get<CfOp::CallInfo>(cf_op.info).target_inputs.reserve(count_used_static_vars);
                    break;
                case CFCInstruction::syscall:
                    std::get<CfOp::SyscallInfo>(cf_op.info).continuation_mapping.reserve(count_used_static_vars);
                    break;
                default:
                    break;
                }

                // zero extend all f32 to f64 in order to map correctly to the fp statics
                zero_extend_all_f32(cur_bb, mapping, cur_bb->virt_end_addr);

                for (size_t i = 0; i < count_used_static_vars; i++) {
                    auto var = mapping[i];
                    if (var != nullptr) {
                        cf_op.add_target_input(var, i);
                        std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = i;
                    }
                }
            }

            if (cf_op.type == CFCInstruction::call || cf_op.type == CFCInstruction::icall) {
                BasicBlock *cont_block = get_bb(next_addr);
                if (cont_block == nullptr) {
                    if (next_addr > prog->addrs.back()) {
                        cont_block = dummy;
                    } else {
                        needs_bb_start[(next_addr - ir->virt_bb_start_addr) / 2] = true;
                    }
                } else if (cont_block->virt_start_addr != next_addr) {
                    split_basic_block(cont_block, next_addr, prog->elf_base.get());
                }
            }

            if (next_bb && next_bb != dummy) {
                if (cf_op.target() == nullptr) {
                    cur_bb->successors.push_back(next_bb);
                    next_bb->predecessors.push_back(cur_bb);
                    cf_op.set_target(next_bb);
                } else {
                    assert(cf_op.target() == next_bb);
                }

                if (next_bb->virt_start_addr != jmp_addr) {
                    // need to split
                    if (next_bb == cur_bb) {
                        cur_bb = split_basic_block(next_bb, jmp_addr, prog->elf_base.get());
                    } else {
                        split_basic_block(next_bb, jmp_addr, prog->elf_base.get());
                    }
                }
            } else {
                // need a bb at jmp_addr
                if (jmp_addr >= ir->virt_bb_start_addr && jmp_addr <= ir->virt_bb_end_addr) {
                    needs_bb_start[(jmp_addr - ir->virt_bb_start_addr) / 2] = true;
                }
            }
        }
        cur_bb->variables.shrink_to_fit();
        cur_bb->control_flow_ops.shrink_to_fit();
        cur_bb = nullptr;
    }

    ir->entry_block = get_bb(prog->elf_base->header.e_entry)->id;
    postprocess(prog);
}

void Lifter::postprocess(Program *prog) {
    // store ijumps for later target backtracking
    std::vector<CfOp *> unprocessed_ijumps;

    // set all jump targets and remove guessed ijumps
    for (auto &bb : ir->basic_blocks) {
        for (auto &cf_op : bb->control_flow_ops) {
            if (cf_op.type == CFCInstruction::unreachable || cf_op.type == CFCInstruction::_return || cf_op.type == CFCInstruction::jump_interpreter) {
                continue;
            }

            auto &lifter_info = std::get<CfOp::LifterInfo>(cf_op.lifter_info);
            if ((cf_op.type == CFCInstruction::call || cf_op.type == CFCInstruction::icall) && lifter_info.instr_addr) {
                auto *target_bb = get_bb(lifter_info.instr_addr + 2);
                if (!target_bb)
                    target_bb = get_bb(lifter_info.instr_addr + 4);
                if (target_bb) {
                    if (cf_op.type == CFCInstruction::call) {
                        std::get<CfOp::CallInfo>(cf_op.info).continuation_block = target_bb;
                    } else {
                        std::get<CfOp::ICallInfo>(cf_op.info).continuation_block = target_bb;
                    }
                    if (std::find(bb->successors.begin(), bb->successors.end(), target_bb) == bb->successors.end()) {
                        bb->successors.push_back(target_bb);
                    }
                    if (std::find(target_bb->predecessors.begin(), target_bb->predecessors.end(), bb.get()) == target_bb->predecessors.end()) {
                        target_bb->predecessors.push_back(bb.get());
                    }
                    target_bb->gen_info.call_cont_block = true;
                } else {
                    auto *cont_bb = ir->add_basic_block(lifter_info.instr_addr + 2);
                    cont_bb->set_virt_end_addr(lifter_info.instr_addr + 2);
                    cont_bb->add_cf_op(CFCInstruction::unreachable, nullptr);
                    cont_bb->predecessors.emplace_back(bb.get());
                    bb->successors.emplace_back(cont_bb);
                    cont_bb->gen_info.call_cont_block = true;
                    if (cf_op.type == CFCInstruction::call) {
                        std::get<CfOp::CallInfo>(cf_op.info).continuation_block = cont_bb;
                    } else {
                        std::get<CfOp::ICallInfo>(cf_op.info).continuation_block = cont_bb;
                    }
                }
            }

            if (cf_op.type == CFCInstruction::ijump || cf_op.type == CFCInstruction::icall) {
                unprocessed_ijumps.emplace_back(&cf_op);
                continue;
            }

            auto *cur_target = cf_op.target();
            if (cur_target && cur_target != dummy) {
                if (cf_op.type == CFCInstruction::call) {
                    cur_target->gen_info.call_target = true;
                }
                continue;
            }

            if (lifter_info.jump_addr) {
                auto *target_bb = get_bb(lifter_info.jump_addr);
                if (target_bb) {
                    if (cur_target) {
                        cur_target->predecessors.erase(std::find(cur_target->predecessors.begin(), cur_target->predecessors.end(), bb.get()));
                        bb->successors.erase(std::find(bb->successors.begin(), bb->successors.end(), cur_target));
                    }

                    cf_op.set_target(target_bb);
                    if (std::find(bb->successors.begin(), bb->successors.end(), target_bb) == bb->successors.end()) {
                        bb->successors.push_back(target_bb);
                    }
                    if (std::find(target_bb->predecessors.begin(), target_bb->predecessors.end(), bb.get()) == target_bb->predecessors.end()) {
                        target_bb->predecessors.push_back(bb.get());
                    }

                    if (cf_op.type == CFCInstruction::call) {
                        target_bb->gen_info.call_target = true;
                    }
                } else {
                    cf_op.type = CFCInstruction::unreachable;
                    cf_op.info = std::monostate{};
                }
            } else if (!lifter_info.jump_addr) {
                cf_op.type = CFCInstruction::unreachable;
                cf_op.info = std::monostate{};
            }
        }
    }

    /* Replace any remaining unresolved */
    for (auto bb : dummy->predecessors) {
        for (auto &cf_op : bb->control_flow_ops) {
            if (cf_op.target() == dummy && cf_op.type != CFCInstruction::syscall) {
                /* This jump could not be resolved, and won't be able to resolve it at runtime */
                cf_op.type = CFCInstruction::unreachable;
                cf_op.info = std::monostate{};
                bb->successors.erase(std::find(bb->successors.begin(), bb->successors.end(), dummy));
            }
        }
    }
    dummy->predecessors.clear();

    /* TODO: this isn't very nice: make all relative immediates actually relative */
    for (auto &bb : ir->basic_blocks) {
        for (auto &var : bb->variables) {
            if (std::holds_alternative<SSAVar::ImmInfo>(var->info) && std::get<SSAVar::ImmInfo>(var->info).binary_relative) {
                std::get<SSAVar::ImmInfo>(var->info).val -= ir->base_addr;
            }
        }
    }

    // find more basic block entrypoints from ijumps
    process_ijumps(unprocessed_ijumps, prog->elf_base.get());

    // add setup_stack block
    auto *program_entry = ir->basic_blocks[ir->entry_block].get();
    auto *entry_block = ir->add_basic_block(0, "___STACK_ENTRY");
    auto &cf_op = entry_block->add_cf_op(CFCInstruction::jump, program_entry);
    for (size_t i = 1; i < count_used_static_vars; ++i) {
        if (i == 2) {
            // stack_var
            auto *var = entry_block->add_var(Type::i64, 0, 2);
            auto op = std::make_unique<Operation>(Instruction::setup_stack);
            op->set_outputs(var);
            var->set_op(std::move(op));
            cf_op.add_target_input(var, 2);
            continue;
        }

        auto *var = entry_block->add_var_from_static(i, 0);
        cf_op.add_target_input(var, i);
    }

    ir->entry_block = entry_block->id;
}
