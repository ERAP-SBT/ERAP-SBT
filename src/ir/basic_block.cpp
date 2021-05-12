#include "../../include/ir/basic_block.h"
#include "ir/ir.h"

void BasicBlock::add_var(std::shared_ptr<Variable> new_var)
{
    variables.push_back(std::move(new_var));
}

void BasicBlock::add_successor(std::weak_ptr<BasicBlock> &new_successor)
{
    successors.push_back(new_successor);
}

void BasicBlock::add_predecessor(std::weak_ptr<BasicBlock> &new_predecessor)
{
    predecessors.push_back(new_predecessor);
}

void BasicBlock::add_successor_mapping(std::weak_ptr<BasicBlock> &block, std::vector<MapEntry::VarMapping> &&mapping) 
{
    succ_mapping.emplace_back(block, std::move(mapping));
}

/*void BasicBlock::add_predecessor_mapping(std::weak_ptr<BasicBlock> &block, std::vector<MapEntry::VarMapping> &&mapping) 
{
    pred_mapping.emplace_back(block, std::move(mapping));
}*/

void BasicBlock::print(std::ostream &stream) const {
    stream << "block b" << id << "(";
    // not very cool rn
    {
        auto first = true;
        for (const auto& var : inputs) {
            if (!first) {
                stream << ", ";
            } else {
                first = false;
            }
            if (var->is_assigned_from_static()) {
              const auto& static_var = ir->get_statics()[var->get_static_idx()];
              stream << var->get_type() << " v" << var->get_id() << " <- @" << static_var.get_name();
            } else
            {
              stream << var->get_type() << " v" << var->get_id();
            }
        }
    }
    stream << ") <= [";
    {
        auto first = true;
        for (const auto& pred : predecessors) {
            if (const auto ptr = pred.lock(); ptr) {
                if (!first) {
                    stream << ", ";
                } else {
                    first = false;
                }
                stream << "b" << ptr->get_id();
            }
        }
    }
    stream << "] {\n";

    for (const auto& var : variables) {
        var->print(stream);
    }

    stream << "} => [TODO]";
}
