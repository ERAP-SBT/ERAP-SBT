#pragma once

#include "operation.h"
#include <memory>
#include <string>
#include <vector>

// forward declaration
class Function;

class BasicBlock
{
    private:
    // unique id, starting from 0.
    int id;
    const std::unique_ptr<CFCOperation> closing_operation;
    const std::shared_ptr<Function> function;
    std::vector<std::unique_ptr<Operation>> operations;
    std::vector<std::shared_ptr<BasicBlock>> predecessors;
    std::vector<std::shared_ptr<BasicBlock>> successors;
    // TODO: add (*inputs*){ static_value_mapping }, (*outputs*){ static_value_mapping }
    // TODO: add per-successor parameter mapping
    // TODO: add default parameter mapping

    public:
    BasicBlock(int, std::unique_ptr<CFCOperation>, std::shared_ptr<Function>);

    ~BasicBlock();

    void add_operation(std::unique_ptr<Operation>);

    void add_successor(std::shared_ptr<BasicBlock> &new_successor);

    void add_predecessor(std::shared_ptr<BasicBlock> &new_predecessor);

    inline const std::vector<std::shared_ptr<BasicBlock>> &get_predecessors() const { return predecessors; }

    inline const std::vector<std::shared_ptr<BasicBlock>> &get_successors() const { return successors; }

    inline int get_id() const { return id; }

    inline const std::vector<std::unique_ptr<Operation>> &get_operations() const { return operations; }

    inline const std::unique_ptr<CFCOperation> &get_closing_operation() const { return closing_operation; }

    inline std::shared_ptr<Function> get_function() const { return function; }
};