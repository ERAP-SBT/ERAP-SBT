#pragma once

#include "basic_block.h"
#include <vector>

struct Function
{
    size_t id;
    std::vector<BasicBlock *> blocks;

    Function(const size_t id) : id(id) { }

    void add_block(BasicBlock *block) { blocks.push_back(block); }

    void print(std::ostream &, const IR *) const;
    void print_name(std::ostream &, const IR *) const;
};
