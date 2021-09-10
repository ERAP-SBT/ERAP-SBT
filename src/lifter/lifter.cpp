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
    dummy = ir->add_basic_block(0, "Dummy Basic Block");

    if (prog->elf_base->section_headers.empty()) {
        // preload all instructions which are located "in" the program headers which are loaded, executable and readable
        for (auto &prog_hdr : prog->elf_base->program_headers) {
            if (prog_hdr.p_type == PT_LOAD && prog_hdr.p_flags & PF_X && prog_hdr.p_flags & PF_R) {
                prog->load_instrs(prog->elf_base->file_content.data() + prog_hdr.p_offset, prog_hdr.p_filesz, prog_hdr.p_vaddr);
            }
        }
    } else {
        // parse all executable sections (only if section information is contained in the elf binary)
        for (auto &sh_hdr : prog->elf_base->section_headers) {
            if (sh_hdr.sh_type & SHT_PROGBITS && sh_hdr.sh_flags & SHF_EXECINSTR) {
                prog->load_instrs(prog->elf_base->file_content.data() + sh_hdr.sh_offset, sh_hdr.sh_size, sh_hdr.sh_addr);
            }
        }
    }

    ir->setup_bb_addr_vec(prog->addrs[0], prog->addrs.back());

    needs_bb_start.clear();
    needs_bb_start.resize(ir->virt_bb_ptrs.size());
    needs_bb_start[(prog->elf_base->header.e_entry - ir->virt_bb_start_addr)] = true;

    for (size_t i = 0; i < 32; i++) {
        ir->add_static(Type::i64);
    }
    // add the memory token as the last static slot
    ir->add_static(Type::mt);

    BasicBlock *cur_bb = nullptr;
    reg_map mapping;
    const auto create_new_bb = [this, prog, &cur_bb, &mapping](uint64_t prev_addr, uint64_t virt_addr) {
        auto *new_bb = ir->add_basic_block(virt_addr, prog->elf_base->symbol_str_at_addr(virt_addr).value_or(""));

        if (cur_bb) {
            // create jump to new bb
            auto &cf_op = cur_bb->add_cf_op(CFCInstruction::jump, new_bb, prev_addr, virt_addr);
            for (size_t i = 0; i < mapping.size(); i++) {
                auto var = mapping[i];
                if (var != nullptr) {
                    cf_op.add_target_input(var, i);
                    std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = i;
                }
            }
            assert(cf_op.target_inputs().size() == 32);
            cur_bb->set_virt_end_addr(prev_addr);
        }

        // load ssa variables from static vars and fill mapping
        for (size_t i = 0; i < 33; i++) {
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
        if (!cur_bb && instr.instr.mnem == FRV_INVALID) {
            // skip
            continue;
        }
        if (cur_bb && instr.instr.mnem == FRV_INVALID) {
            cur_bb->add_cf_op(CFCInstruction::unreachable, nullptr, virt_addr);
            cur_bb = nullptr;
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

            uint64_t jmp_addr = std::get<CfOp::LifterInfo>(cf_op.lifter_info).jump_addr;
            BasicBlock *next_bb;

            if (jmp_addr == 0) {
                DEBUG_LOG("Encountered indirect jump / unknown jump target. Trying to guess the correct jump address...");
                auto addr = backtrace_jmp_addr(&cf_op, cur_bb);
                if (!addr.has_value()) {
                    DEBUG_LOG("-> Address backtracking failed, skipping branch.");
                    next_bb = dummy;
                } else {
                    std::stringstream str;
                    str << " -> Found a possible address: 0x" << std::hex << addr.value();
                    DEBUG_LOG(str.str());

                    jmp_addr = addr.value();
                    next_bb = get_bb(jmp_addr);
                }
            } else {
                next_bb = get_bb(jmp_addr);
            }

            // when we split a bblock the target_inputs get filled by split_basic_block
            if (cf_op.target_inputs().empty()) {
                for (size_t i = 0; i < mapping.size(); i++) {
                    auto var = mapping[i];
                    if (var != nullptr) {
                        cf_op.add_target_input(var, i);
                        std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = i;
                    }
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
        cur_bb = nullptr;
    }

    ir->entry_block = get_bb(prog->elf_base->header.e_entry)->id;
    postprocess();
}

void Lifter::postprocess() {

    /* Remove any guessed ijumps */
    for (auto &bb : ir->basic_blocks) {
        for (auto &cf_op : bb->control_flow_ops) {
            if (cf_op.type == CFCInstruction::ijump) {
                cf_op.set_target(nullptr);
            }
        }
    }

    // set all jump targets
    for (auto &bb : ir->basic_blocks) {
        for (auto &cf_op : bb->control_flow_ops) {
            auto &lifter_info = std::get<CfOp::LifterInfo>(cf_op.lifter_info);
            if (cf_op.type != CFCInstruction::ijump && cf_op.type != CFCInstruction::icall && cf_op.type != CFCInstruction::unreachable && lifter_info.jump_addr) {
                auto *target_bb = get_bb(lifter_info.jump_addr);
                if (target_bb) {
                    cf_op.set_target(target_bb);
                    bb->successors.push_back(target_bb);
                    target_bb->predecessors.push_back(bb.get());
                } else {
                    cf_op.type = CFCInstruction::unreachable;
                }
            }
            // TODO: replace with unreachable if jmp_addr = 0?
            if (cf_op.type == CFCInstruction::call && lifter_info.continuation_addr) {
                auto *target_bb = get_bb(lifter_info.continuation_addr);
                if (target_bb) {
                    std::get<CfOp::CallInfo>(cf_op.info).continuation_block = target_bb;
                    bb->successors.push_back(target_bb);
                    target_bb->predecessors.push_back(bb.get());
                } else {
                    // TODO: add block with unreachable cfop?
                }
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
            }
        }
    }

    /* TODO: this isn't very nice: make all relative immediates actually relative */
    for (auto &bb : ir->basic_blocks) {
        for (auto &var : bb->variables) {
            if (std::holds_alternative<SSAVar::ImmInfo>(var->info) && std::get<SSAVar::ImmInfo>(var->info).binary_relative) {
                std::get<SSAVar::ImmInfo>(var->info).val -= ir->base_addr;
            }
        }
    }

    // add setup_stack block
    auto *program_entry = ir->basic_blocks[ir->entry_block].get();
    auto *entry_block = ir->add_basic_block(0, "___STACK_ENTRY");
    auto &cf_op = entry_block->add_cf_op(CFCInstruction::jump, program_entry);
    for (size_t i = 1; i <= 32; ++i) {
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
