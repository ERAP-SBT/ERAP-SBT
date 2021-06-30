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
    std::unordered_set<uint64_t> bb_start_addrs;

    std::vector<std::unique_ptr<Function>> functions;
    std::vector<StaticMapper> statics;

    size_t cur_block_id = 0;
    size_t cur_func_id = 0;
    size_t entry_block = 0;

    BasicBlock *add_basic_block(const size_t virt_start_addr = 0, std::string dbg_name = {}) {
        auto block = std::make_unique<BasicBlock>(this, cur_block_id++, virt_start_addr, dbg_name);
        const auto ptr = block.get();
        basic_blocks.push_back(std::move(block));
        bb_start_addrs.emplace(virt_start_addr);
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

    void print(std::ostream &) const;
};
