#pragma once

#include "basic_block.h"
#include "function.h"

#include <cassert>
#include <memory>
#include <ostream>
#include <unordered_set>
#include <vector>

struct IR {
    std::vector<std::unique_ptr<BasicBlock>> basic_blocks;

    std::vector<BasicBlock *> virt_bb_ptrs;
    uint64_t base_addr;
    uint64_t load_size;
    uint64_t phdr_off;
    uint64_t phdr_size;
    uint64_t phdr_num;
    uint64_t p_entry_addr;
    uint64_t virt_bb_start_addr;
    uint64_t virt_bb_end_addr;

    std::vector<std::unique_ptr<Function>> functions;
    std::vector<StaticMapper> statics;

    size_t cur_block_id = 0;
    size_t cur_func_id = 0;
    size_t entry_block = 0;

    BasicBlock *add_basic_block(const size_t virt_start_addr = 0, const std::string &dbg_name = {}) {
        auto block = std::make_unique<BasicBlock>(this, cur_block_id++, virt_start_addr, dbg_name);
        const auto ptr = block.get();
        basic_blocks.push_back(std::move(block));

        if (virt_start_addr != 0 && virt_start_addr >= virt_bb_start_addr) {
            virt_bb_ptrs.at((virt_start_addr - virt_bb_start_addr) / 2) = ptr;
        }
        return ptr;
    }

    Function *add_func() {
        auto func = std::make_unique<Function>(cur_func_id++);
        const auto ptr = func.get();
        functions.push_back(std::move(func));
        return ptr;
    }

    size_t add_static(const Type type) {
        assert(type != Type::imm);
        const auto id = statics.size();
        statics.emplace_back(id, type);
        return id;
    }

    BasicBlock *bb_at_addr(uint64_t addr) {
        const auto off = (addr - virt_bb_start_addr) / 2;
        if (off >= virt_bb_ptrs.size()) {
            return nullptr;
        }
        return virt_bb_ptrs[off];
    }

    void setup_bb_addr_vec(uint64_t start_addr, uint64_t end_addr) {
        virt_bb_start_addr = start_addr;
        virt_bb_end_addr = end_addr;

        virt_bb_ptrs = std::vector<BasicBlock *>((end_addr - start_addr) / 2 + 1);
    }

    bool verify(std::vector<std::string> &messages_out) const;

    void print(std::ostream &) const;
};
