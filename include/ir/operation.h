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
    const std::shared_ptr<Instruction> instruction;
    const std::vector<std::shared_ptr<Variable>> input_variables;
    const std::vector<std::shared_ptr<Variable>> output_variables;
    const std::shared_ptr<BasicBlock> current_block;

    // for loading immediates, only available with `immediate` operation
    const int64_t immediate;

    public:
    // TODO: add per-instruction checks for correct syntax
    Operation(std::shared_ptr<Instruction> instr, std::vector<std::shared_ptr<Variable>> &input_vars, std::vector<std::shared_ptr<Variable>> &output_vars, std::shared_ptr<BasicBlock> bb);
    Operation(int64_t imm, std::vector<std::shared_ptr<Variable>> &input_vars, std::vector<std::shared_ptr<Variable>> &output_vars, std::shared_ptr<BasicBlock> bb);
    ~Operation();

    // Getters
    inline const std::shared_ptr<Instruction> &get_instruction() const { return instruction; };
    inline const std::vector<std::shared_ptr<Variable>> &get_input_variables() const { return input_variables; };
    inline const std::vector<std::shared_ptr<Variable>> &get_output_variables() const { return output_variables; };
    inline std::shared_ptr<BasicBlock> get_current_block() const { return current_block; }
};

class CFCOperation
{
    private:
    const std::shared_ptr<CFCInstruction> cfc_instruction;
    const std::vector<std::shared_ptr<Variable>> input_variables;
    const std::shared_ptr<BasicBlock> current_block;

    // optional array of jump / call targets, useful for predecessor / successor identification
    std::vector<std::shared_ptr<BasicBlock>> targets;

    public:
    CFCOperation(std::shared_ptr<CFCInstruction>, std::vector<std::shared_ptr<Variable>>, std::shared_ptr<BasicBlock>);
    ~CFCOperation();

    void add_new_target(std::shared_ptr<BasicBlock> &);

    // Getters
    inline const std::shared_ptr<CFCInstruction> &get_cfc_instruction() const { return cfc_instruction; };
    inline const std::vector<std::shared_ptr<Variable>> &get_input_variables() const { return input_variables; };
    inline std::shared_ptr<BasicBlock> get_current_block() const { return current_block; }
};