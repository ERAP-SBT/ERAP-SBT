#pragma once

#include "basic_block.h"
#include <memory>
#include <vector>

class Function
{
    private:
    const int id;
    const std::shared_ptr<BasicBlock> entry_block;
    std::vector<std::shared_ptr<BasicBlock>> blocks;

    public:
    Function(int, std::shared_ptr<BasicBlock>);
    ~Function();

    void add_block(const std::shared_ptr<BasicBlock> &bb);

    inline int get_id() const { return id; }

    inline std::vector<std::shared_ptr<BasicBlock>> get_blocks() const { return blocks; }

    inline std::shared_ptr<BasicBlock> get_entry_block() const { return entry_block; }
};