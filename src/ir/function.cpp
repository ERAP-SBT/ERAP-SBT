#include "../../include/ir/function.h"

void Function::add_block(const std::shared_ptr<BasicBlock>& bb)
{
    blocks.push_back(bb);
}
