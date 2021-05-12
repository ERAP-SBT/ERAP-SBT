#pragma once

#include "operation.h"
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include <optional>

// forward declaration
class Function;
class IR;

class BasicBlock
{
  public:
    struct MapEntry {
      struct VarMapping {
        size_t static_var;
        std::shared_ptr<Variable> var;
      };

      std::weak_ptr<BasicBlock> block;
      std::vector<VarMapping> mapping;

      MapEntry(std::weak_ptr<BasicBlock> &block, std::vector<VarMapping> &&mapping) : block(block), mapping(std::move(mapping)) {}
    };

  private:
    IR* ir;
    // unique id, starting from 0.
    size_t id;
    size_t cur_ssa_id = 0;
    std::optional<CFCOperation> closing_operation;
    const std::weak_ptr<Function> function;
    // TODO: do we need to keep track of ops here or is it enough to keep a list of vars in the "right order"?  (are we even interested in the right order or just var dependencies?)
    // since this way it's very easy for constant folding to just remove ops that do not need to occur
    std::vector<std::shared_ptr<Variable>> inputs;
    std::vector<std::shared_ptr<Variable>> variables;
    std::vector<std::weak_ptr<BasicBlock>> predecessors;
    std::vector<std::weak_ptr<BasicBlock>> successors;

    // TODO: do we actually have a pred mapping like this or only a list of predecessors and a defined list of inputs?
    std::vector<MapEntry> /*pred_mapping,*/ succ_mapping;

    public:
    BasicBlock(IR* ir, size_t id, std::weak_ptr<Function> function)
    : ir(ir), id(id),
      function(function) { }

    void set_closing_op(CFCOperation op) {
      closing_operation = op;
    }

    void add_input(std::shared_ptr<Variable> input)
    {
      inputs.push_back(input);
    }
	
    void add_var(std::shared_ptr<Variable>);
    void add_successor(std::weak_ptr<BasicBlock> &);
    void add_predecessor(std::weak_ptr<BasicBlock> &);

    //void add_predecessor_mapping(std::weak_ptr<BasicBlock>& block, std::vector<MapEntry::VarMapping> &&mapping);
    void add_successor_mapping(std::weak_ptr<BasicBlock>& block, std::vector<MapEntry::VarMapping> &&mapping);

    // Getters
    const std::vector<std::weak_ptr<BasicBlock>> &get_predecessors() const { return predecessors; }
    const std::vector<std::weak_ptr<BasicBlock>> &get_successors() const { return successors; }
    size_t get_id() const { return id; }
    const std::vector<std::shared_ptr<Variable>> &get_variables() const { return variables; }
    std::weak_ptr<Function> get_function() const { return function; }

    size_t get_next_ssa_id() { return cur_ssa_id++; }

    void print(std::ostream&) const;
};