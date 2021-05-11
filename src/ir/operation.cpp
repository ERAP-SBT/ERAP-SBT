#include "../../include/ir/operation.h"

Operation::Operation(std::shared_ptr<Instruction> instr, std::vector<std::shared_ptr<Variable>> &input_vars, std::vector<std::shared_ptr<Variable>> &output_vars, std::shared_ptr<BasicBlock> bb)
    : instruction(std::move(instr)),
      input_variables(std::move(input_vars)),
      output_variables(std::move(output_vars)),
      current_block(std::move(bb)),
      immediate(0)
{ }

Operation::Operation(int64_t imm, std::vector<std::shared_ptr<Variable>> &input_vars, std::vector<std::shared_ptr<Variable>> &output_vars, std::shared_ptr<BasicBlock> bb)
    : instruction(new Instruction{Instruction::immediate}),
      input_variables(std::move(input_vars)),
      output_variables(std::move(output_vars)),
      current_block(std::move(bb)),
      immediate(imm)
{ }

Operation::~Operation() = default;

CFCOperation::CFCOperation(std::shared_ptr<CFCInstruction> instr, std::vector<std::shared_ptr<Variable>> input_vars, std::shared_ptr<BasicBlock> bb)
    : cfc_instruction(std::move(instr)),
      input_variables(std::move(input_vars)),
      current_block(std::move(bb)),
      targets()
{ }
CFCOperation::~CFCOperation() = default;

void CFCOperation::add_new_target(std::shared_ptr<BasicBlock> &target)
{
    targets.push_back(target);
}
