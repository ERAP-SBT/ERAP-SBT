#pragma once

#include "operation.h"
#include <map>
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

    // maps for static value mapping. map<predecessor, map<ssa_variable, static_mapper>>
    // if the key / predecessor is "nullptr", this mapping is used as the default mapping.
    std::map<std::shared_ptr<BasicBlock>, std::map<std::shared_ptr<Variable>, std::shared_ptr<StaticMapper>>> predecessor_static_mapping;
    std::map<std::shared_ptr<BasicBlock>, std::map<std::shared_ptr<Variable>, std::shared_ptr<StaticMapper>>> successor_static_mapping;

    public:
    BasicBlock(int, std::unique_ptr<CFCOperation>, std::shared_ptr<Function>);
    ~BasicBlock();

    void add_operation(std::unique_ptr<Operation>);
    void add_successor(std::shared_ptr<BasicBlock> &);
    void add_predecessor(std::shared_ptr<BasicBlock> &);

    void add_predecessor_mapping(std::shared_ptr<BasicBlock> &, std::shared_ptr<Variable> &, std::shared_ptr<StaticMapper> &);
    void add_successor_mapping(std::shared_ptr<BasicBlock> &, std::shared_ptr<Variable> &, std::shared_ptr<StaticMapper> &);

    // Getters
    inline const std::vector<std::shared_ptr<BasicBlock>> &get_predecessors() const { return predecessors; }
    inline const std::vector<std::shared_ptr<BasicBlock>> &get_successors() const { return successors; }
    inline int get_id() const { return id; }
    inline const std::vector<std::unique_ptr<Operation>> &get_operations() const { return operations; }
    inline const std::unique_ptr<CFCOperation> &get_closing_operation() const { return closing_operation; }
    inline std::shared_ptr<Function> get_function() const { return function; }
    inline std::map<std::shared_ptr<Variable>, std::shared_ptr<StaticMapper>> get_successor_mapping(std::shared_ptr<BasicBlock> &successor_key) { return successor_static_mapping[successor_key]; }
    inline std::map<std::shared_ptr<Variable>, std::shared_ptr<StaticMapper>> get_predecessor_mapping(std::shared_ptr<BasicBlock> &predecessor_key)
    {
        return predecessor_static_mapping[predecessor_key];
    }
};