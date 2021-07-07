#include <frvdec.h>
#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::lift(Program *prog) {
    assert(prog->elf_base->base_addr <= prog->elf_base->load_end_addr);
    ir->base_addr = prog->elf_base->base_addr;
    ir->load_size = prog->elf_base->load_end_addr - prog->elf_base->base_addr;
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

    ir->setup_bb_addr_vec(prog->addrs.at(0), prog->addrs.back());

    uint64_t start_addr = prog->elf_base->header.e_entry;
    Function *curr_fun = ir->add_func();

    for (size_t i = 0; i < 32; i++) {
        ir->add_static(Type::i64);
    }
    // add the memory token as the last static slot
    ir->add_static(Type::mt);

    BasicBlock *first_bb = ir->add_basic_block(start_addr, prog->elf_base->symbol_str_at_addr(start_addr).value_or(""));
    {
        std::stringstream str;
        str << "Starting new basicblock #0x" << std::hex << first_bb->id;
        DEBUG_LOG(str.str());
    }
    ir->entry_block = first_bb->id;

    lift_rec(prog, curr_fun, start_addr, std::nullopt, first_bb);

    if (LIFT_ALL_LOAD) {
        std::vector<uint64_t> unparsed_addrs = std::vector<uint64_t>(prog->addrs);

        for (const auto &bb : ir->basic_blocks) {
            const uint64_t virt_start_addr = bb->virt_start_addr;
            const uint64_t virt_end_addr = bb->virt_end_addr;

            if (virt_start_addr && virt_end_addr) {
                const auto start_i = (size_t)std::distance(unparsed_addrs.begin(), std::find(unparsed_addrs.begin(), unparsed_addrs.end(), virt_start_addr));
                while (unparsed_addrs.at(start_i) != virt_end_addr) {
                    unparsed_addrs.erase(std::next(unparsed_addrs.begin(), (long)start_i));
                }
                // don't forget to also erase the end address
                if (unparsed_addrs.size() > start_i) {
                    unparsed_addrs.erase(std::next(unparsed_addrs.begin(), (long)start_i));
                }
            }
        }

        size_t last_bb_id = ir->basic_blocks.back()->id;

        while (!unparsed_addrs.empty()) {
            BasicBlock *new_bb = ir->add_basic_block(unparsed_addrs.at(0), prog->elf_base->symbol_str_at_addr(unparsed_addrs.at(0)).value_or(""));
            {
                std::stringstream str;
                str << "Starting new basicblock #0x" << std::hex << new_bb->id;
                DEBUG_LOG(str.str());
            }
            lift_rec(prog, curr_fun, unparsed_addrs.at(0), std::nullopt, new_bb);

            for (auto &bb : ir->basic_blocks) {
                if (bb->id > last_bb_id && bb->virt_start_addr && bb->virt_end_addr) {
                    auto start_i = (size_t)std::distance(unparsed_addrs.begin(), std::find(unparsed_addrs.begin(), unparsed_addrs.end(), bb->virt_start_addr));
                    while (unparsed_addrs.size() > start_i && unparsed_addrs.at(start_i) != bb->virt_end_addr) {
                        unparsed_addrs.erase(std::next(unparsed_addrs.begin(), (long)start_i));
                    }
                    // don't forget to also erase the end address
                    if (unparsed_addrs.size() > start_i) {
                        unparsed_addrs.erase(std::next(unparsed_addrs.begin(), (long)start_i));
                    }
                    last_bb_id = bb->id;
                }
            }
        }
    }

    // TODO: move this post-processing to separate function
    for (auto bb : dummy->predecessors) {
        for (auto &cfOp : bb->control_flow_ops) {
            if (cfOp.target() == dummy) {
                cfOp.type = CFCInstruction::unreachable;
                cfOp.info = std::monostate{};
            }
        }
    }
    for (auto &bb : ir->basic_blocks) {
        for (auto &var : bb->variables) {
            if (std::holds_alternative<SSAVar::ImmInfo>(var->info) && std::get<SSAVar::ImmInfo>(var->info).binary_relative) {
                std::get<SSAVar::ImmInfo>(var->info).val -= ir->base_addr;
            }
        }
    }
}

void Lifter::lift_rec(Program *prog, Function *func, uint64_t start_addr, std::optional<size_t> addr_idx, BasicBlock *curr_bb) {
    // search for the address index in the program vectors
    size_t start_i;
    if (addr_idx.has_value()) {
        start_i = addr_idx.value();
    } else {
        auto it = std::lower_bound(prog->addrs.begin(), prog->addrs.end(), start_addr);
        if (it == prog->addrs.end() || *it != start_addr) {
            std::cerr << "Couldn't find parsed instructions at given address. Aborting recursive parsing step." << std::endl;
            return;
        }
        start_i = std::distance(prog->addrs.begin(), it);
    }

    // init register mapping array
    reg_map mapping;

    // load ssa variables from static vars and fill mapping
    for (size_t i = 0; i < 33; i++) {
        if (i != ZERO_IDX) {
            mapping.at(i) = curr_bb->add_var_from_static(i, start_addr);
        } else {
            mapping.at(i) = nullptr;
        }
    }

    for (size_t i = start_i; i < start_i + 10000 && i < prog->addrs.size(); i++) {
        if (prog->data.at(i).index() != 1) {
            /* ignore non RV64 Instructions */
            continue;
        }

        // ignore the first address, otherwise we always find the existing curr_bb basic block
        if (i > start_i) {
            // test if another parsed or unparsed basic block starts at this instruction to avoid duplicated instructions
            const auto jmp_addr = prog->addrs.at(i);
            auto bb = ir->bb_at_addr(jmp_addr);
            if (bb) {
                const auto instr_addr = prog->addrs.at(i - 1);
                SSAVar *addr_imm = curr_bb->add_var_imm((int64_t)jmp_addr, instr_addr, true);
                CfOp &jmp = curr_bb->add_cf_op(CFCInstruction::jump, bb, instr_addr, bb->virt_start_addr);
                jmp.set_inputs(addr_imm);
                curr_bb->set_virt_end_addr(instr_addr);
                break;
            }
        }

        const RV64Inst instr = std::get<RV64Inst>(prog->data.at(i));

        // the next_addr is used for CfOps which require a return address / address of the next instruction
        uint64_t next_addr;

        // test if the next address is outside of the already parsed addresses
        if (i < prog->addrs.size() - 1) {
            next_addr = prog->addrs.at(i + 1);
        } else {
            next_addr = prog->addrs.at(i) + 2;
        }
        parse_instruction(instr, curr_bb, mapping, prog->addrs.at(i), next_addr);
        if (!curr_bb->control_flow_ops.empty()) {
            curr_bb->set_virt_end_addr(prog->addrs.at(i));
            break;
        }

        // for now, we stop after 10000 instructions / data elements in one basic block
        if (i + 1 >= start_i + 10000 || i + 1 >= prog->addrs.size()) {
            curr_bb->set_virt_end_addr(prog->addrs.at(i));
            break;
        }
    }
    assert(curr_bb->virt_start_addr != 0);
    assert(curr_bb->virt_end_addr != 0);

    // store the entry addresses where the parsing continues next
    std::vector<std::pair<uint64_t, BasicBlock *>> next_entrypoints;
    // ...and store jump addresses with their corresponding hit basic blocks which should be split
    std::vector<std::pair<uint64_t, BasicBlock *>> to_split;

    for (CfOp &cfOp : curr_bb->control_flow_ops) {
        uint64_t jmp_addr = std::get<CfOp::LifterInfo>(cfOp.lifter_info).jump_addr;
        BasicBlock *next_bb;

        if (jmp_addr == 0) {
            DEBUG_LOG("Encountered indirect jump / unknown jump target. Trying to guess the correct jump address...");
            auto addr = backtrace_jmp_addr(&cfOp, curr_bb);
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

        bool bb_exists = false;
        if (next_bb == nullptr) {
            if (jmp_addr > prog->addrs.back()) {
                next_bb = dummy;
            } else {
                next_bb = ir->add_basic_block(jmp_addr, prog->elf_base->symbol_str_at_addr(jmp_addr).value_or(""));
                func->add_block(next_bb);
            }
        } else {
            bb_exists = true;
        }

        // TODO: This is a problem if we split the basic block later on
        curr_bb->successors.push_back(next_bb);
        next_bb->predecessors.push_back(curr_bb);
        cfOp.set_target(next_bb);
        cfOp.source = curr_bb;

        for (size_t i = 0; i < mapping.size(); i++) {
            auto var = mapping.at(i);
            if (var != nullptr) {
                cfOp.add_target_input(var, i);
                std::get<SSAVar::LifterInfo>(var->lifter_info).static_id = i;
            }
        }

        if (next_bb != dummy) {
            if (bb_exists) {
                if (next_bb->virt_start_addr != jmp_addr) {
                    // the jump address is inside a parsed basic block -> split the block
                    // the split basic block function needs the cfOps fully initialized.
                    to_split.emplace_back(jmp_addr, next_bb);
                }
            } else {
                next_entrypoints.emplace_back(jmp_addr, next_bb);
            }
        }
    }

    std::for_each(to_split.begin(), to_split.end(), [this, prog](auto &split_tuple) {
        std::stringstream str;
        str << "Splitting basic block #0x" << std::hex << split_tuple.second->id;
        DEBUG_LOG(str.str());
        split_basic_block(split_tuple.second, split_tuple.first, prog->elf_base.get());
    });

    std::for_each(next_entrypoints.begin(), next_entrypoints.end(), [this, prog, func](auto &entrypoint_tuple) {
        std::stringstream str;
        str << "Parsing next basic block #0x" << std::hex << entrypoint_tuple.second->id;
        DEBUG_LOG(str.str());
        lift_rec(prog, func, entrypoint_tuple.first, std::nullopt, entrypoint_tuple.second);
    });
}
