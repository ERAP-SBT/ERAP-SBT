#include <iostream>

#include <lifter/lifter.h>
#include <ir/ir.h>
#include <generator/generator.h>

int main() {
	IR ir = IR{};
	ir.add_static(StaticMapper{"r0", Type::i64});
	ir.add_static(StaticMapper{"r1", Type::i64});
	ir.add_static(StaticMapper{"r2", Type::i64});

	auto basic_block = std::make_shared<BasicBlock>(&ir, ir.next_block_id(), std::weak_ptr<Function>{});
	basic_block->add_input(std::make_shared<Variable>(basic_block->get_next_ssa_id(), Type::i64, 0));
	basic_block->add_input(std::make_shared<Variable>(basic_block->get_next_ssa_id(), Type::i64, 1));

	auto imm1 = std::make_shared<Variable>(basic_block->get_next_ssa_id(), 123);
	auto imm2 = std::make_shared<Variable>(basic_block->get_next_ssa_id(), 1);
	auto var = std::make_shared<Variable>(basic_block->get_next_ssa_id(), Type::i64);
	auto op = std::make_shared<Operation>(Instruction::add, basic_block, imm1, imm2, nullptr, nullptr, var.get());
	var->set_op(op);

	basic_block->add_var(imm1);
	basic_block->add_var(imm2);
	basic_block->add_var(var);

	ir.add_basic_block(basic_block);
	
	ir.print(std::cout);

	return 0;
}
