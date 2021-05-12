#pragma once

#include "basic_block.h"
#include <memory>
#include <vector>

class Function
{
    const size_t id;
    std::vector<std::shared_ptr<BasicBlock>> blocks;

    public:
    Function(size_t id) : id(id), blocks() {}

    void add_block(const std::shared_ptr<BasicBlock> &bb);

    size_t get_id() const { return id; }

    std::vector<std::shared_ptr<BasicBlock>> get_blocks() const { return blocks; }

    void print(std::ostream&) const {}
};