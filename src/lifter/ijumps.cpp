#include <lifter/lifter.h>

using namespace lifter::RV64;

void Lifter::register_jump_address(BasicBlock *jump_bb, uint64_t jmp_addr, ELF64File *elf_base) {
    if (jump_bb->virt_start_addr != jmp_addr) {
        split_basic_block(jump_bb, jmp_addr, elf_base);
    }
}

void Lifter::process_ijumps(std::vector<CfOp *> &unprocessed_ijumps, ELF64File *elf_base) {
    // store jump addresses with their corresponding hit basic blocks which should be split
    std::vector<std::pair<uint64_t, BasicBlock *>> to_split;

    for (auto jump : unprocessed_ijumps) {
        auto &jump_info = std::get<CfOp::IJumpInfo>(jump->info);

        auto addrs = backtrace_jmp_addrs(jump, jump->source);
        if (addrs.empty()) {
            if (jump_info.targets.empty()) {
                addrs.emplace(0);
                DEBUG_LOG("-> Address backtracking failed (no address was found), skipping branch (setting dummy as target block).");
            } else {
                DEBUG_LOG("-> Address backtracking failed (no address was found), skipping branch.");
                continue;
            }
        }
        for (uint64_t jmp_addr : addrs) {
            if (std::find(jump_info.jmp_addrs.begin(), jump_info.jmp_addrs.end(), jmp_addr) != jump_info.jmp_addrs.end()) {
                // jump target already exists
                continue;
            }

            BasicBlock *jump_bb;
            if (jmp_addr == 0) {
                // we weren't able to parse a jump target, but still need the mapping for later use in the generator.
                jump_bb = dummy;
            } else {
                jump_bb = get_bb(jmp_addr);
            }

            if (jump_bb == nullptr) {
                DEBUG_LOG("Couldn't find a basic block at an indirect jump location, skipping.");
                continue;
            }
            register_jump_address(jump_bb, jmp_addr, elf_base);
        }
    }
}
