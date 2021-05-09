#include "../../include/ir/operation.h"

Operation::Operation(Instruction &instr, std::vector<std::shared_ptr<Variable>> &input_vars, std::vector<std::shared_ptr<Variable>> &output_vars, std::shared_ptr<BasicBlock> bb)
    : input_variables(std::move(input_vars)),
      output_variables(std::move(output_vars)),
      current_block(std::move(bb)),
      instruction(instr)
{ }

Operation::~Operation() = default;

CFCOperation::CFCOperation(CFCInstruction &instr, std::vector<std::shared_ptr<Variable>> input_vars, std::shared_ptr<BasicBlock> bb)
    : input_variables(std::move(input_vars)),
      current_block(std::move(bb)),
      cfc_instruction(instr)
{ }

CFCOperation::~CFCOperation() = default;