#pragma once

#include "function.h"
#include "basic_block.h"
#include <memory>
#include <ostream>
#include <vector>

class IR
{
    private:
    std::vector<std::shared_ptr<BasicBlock>> basic_blocks;
    std::vector<std::shared_ptr<Function>> functions;
    std::vector<StaticMapper> statics;

    size_t block_id = 0, func_id = 0;

    public:
    IR() = default;

    void add_function(const std::shared_ptr<Function> &);
		void add_basic_block(std::shared_ptr<BasicBlock> ptr)
		{
	    basic_blocks.push_back(ptr);
		}
    void add_static(StaticMapper&& static_var) {
        statics.push_back(std::move(static_var));
    }

    // Getters
    const std::vector<std::shared_ptr<Function>> &get_functions() const { return functions; }
    const std::vector<StaticMapper> &get_statics() const { return statics; }

    size_t next_block_id() { return block_id++; }
    size_t next_func_id() { return func_id++; }

    void print(std::ostream &) const;
};