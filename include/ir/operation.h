#pragma once

#include "instruction.h"
#include "variable.h"
#include <memory>
#include <vector>

// forward declaration
class BasicBlock;

class Operation
{
    private:
    const Instruction &instruction;
    const std::vector<std::shared_ptr<Variable>> input_variables;
    const std::vector<std::shared_ptr<Variable>> output_variables;
    const std::shared_ptr<BasicBlock> current_block;
    // TODO: add possibility for immediate loading

    public:
    // TODO: add per-instruction checks for correct syntax
    Operation(Instruction &instr, std::vector<std::shared_ptr<Variable>> &input_vars, std::vector<std::shared_ptr<Variable>> &output_vars, std::shared_ptr<BasicBlock> bb);

    ~Operation();

    inline const Instruction &get_instruction() const { return instruction; };
    inline const std::vector<std::shared_ptr<Variable>> &get_input_variables() const { return input_variables; };

    inline const std::vector<std::shared_ptr<Variable>> &get_output_variables() const { return output_variables; };

    inline std::shared_ptr<BasicBlock> get_current_block() const { return current_block; }
};

// TODO: add jump targets
class CFCOperation
{
    private:
    const CFCInstruction cfc_instruction;
    const std::shared_ptr<BasicBlock> current_block;
    const std::vector<std::shared_ptr<Variable>> input_variables;

    public:
    CFCOperation(CFCInstruction &, std::vector<std::shared_ptr<Variable>>, std::shared_ptr<BasicBlock>);

    ~CFCOperation();

    inline const CFCInstruction &get_cfc_instruction() const { return cfc_instruction; };

    inline const std::vector<std::shared_ptr<Variable>> &get_input_variables() const { return input_variables; };

    inline std::shared_ptr<BasicBlock> get_current_block() const { return current_block; }
};