#include "../../include/ir/function.h"

Function::Function(int id, std::shared_ptr<BasicBlock> entry_block) : id(id), entry_block(std::move(entry_block)), blocks(){}

Function::~Function() = default;

void Function::add_block(const std::shared_ptr<BasicBlock>& bb)
{
    blocks.push_back(bb);
}
