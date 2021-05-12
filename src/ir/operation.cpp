#include "../../include/ir/operation.h"

Operation::Operation(Instruction instr, std::shared_ptr<BasicBlock> bb, std::shared_ptr<Variable> op1,
     std::shared_ptr<Variable> op2, std::shared_ptr<Variable> op3, std::shared_ptr<Variable> op4,
      Variable* out1, Variable* out2, Variable* out3)
    : instruction(instr),
      current_block(bb),
      in_vars({op1, op2, op3, op4}), out_vars({out1, out2, out3}) { 
        auto const_eval = true;
        for (const auto& op : in_vars) {
          if (op && op->get_type() != Type::imm) {
            const_eval = false;
            break;
          }
        }
        constant_evaluatable = const_eval;
      }

void Operation::print(std::ostream &stream) const
{
	switch (instruction)
	{
  case Instruction::add:
    stream << "add ";
    in_vars[0]->print_name_type(stream);
    stream << ", ";
		in_vars[1]->print_name_type(stream);
    break;
  default:
    stream << "unk";
    break;
	}
}

void CFCOperation::add_new_target(std::shared_ptr<BasicBlock> &target)
{
    targets.push_back(target);
}
