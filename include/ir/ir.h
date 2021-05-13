#pragma once

#include "basic_block.h"
#include "function.h"
#include <memory>
#include <ostream>
#include <vector>

#define IR_VERSION "0.1"

struct IR
{
    std::vector<std::unique_ptr<BasicBlock>> basic_blocks;
    std::vector<std::unique_ptr<Function>> functions;
    std::vector<StaticMapper> statics;

    size_t cur_block_id = 0;
    size_t cur_func_id  = 0;

    BasicBlock *add_basic_block()
    {
        auto block     = std::make_unique<BasicBlock>(this, cur_block_id++);
        const auto ptr = block.get();
        basic_blocks.push_back(std::move(block));
        return ptr;
    }

    Function *add_func()
    {
        auto func      = std::make_unique<Function>(cur_func_id++);
        const auto ptr = func.get();
        functions.push_back(std::move(func));
        return ptr;
    }

    size_t add_static(const Type type)
    {
        const auto id = statics.size();
        statics.emplace_back(id, type);
        return id;
    }

    void print(std::ostream &) const;
};