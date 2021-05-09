#include "../../include/ir/basic_block.h"

BasicBlock::BasicBlock(int id, std::unique_ptr<CFCOperation> cfc_operation, std::shared_ptr<Function> function)
    : id(id),
      closing_operation(std::move(cfc_operation)),
      function(std::move(function)),
      operations(),
      predecessors(),
      successors()
{ }

BasicBlock::~BasicBlock() = default;

void BasicBlock::add_operation(std::unique_ptr<Operation> new_op)
{
    operations.push_back(std::move(new_op));
}

void BasicBlock::add_successor(std::shared_ptr<BasicBlock> &new_successor)
{
    successors.push_back(new_successor);
}

void BasicBlock::add_predecessor(std::shared_ptr<BasicBlock> &new_predecessor)
{
    predecessors.push_back(new_predecessor);
}
