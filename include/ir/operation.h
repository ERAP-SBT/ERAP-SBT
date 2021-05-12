#pragma once

#include "instruction.h"
#include "variable.h"
#include <memory>
#include <vector>
#include <array>

// forward declaration
class BasicBlock;

class Operation
{
    private:
    const Instruction instruction;
    const std::weak_ptr<BasicBlock> current_block;
    const std::array<std::shared_ptr<Variable>, 4> in_vars;
    const std::array<Variable*, 3> out_vars;

    bool constant_evaluatable = false;

    public:
    // TODO: add per-instruction checks for correct syntax
    Operation(Instruction instr, std::shared_ptr<BasicBlock> bb, std::shared_ptr<Variable> op1 = nullptr,
     std::shared_ptr<Variable> op2 = nullptr, std::shared_ptr<Variable> op3 = nullptr, std::shared_ptr<Variable> op4 = nullptr,
      Variable* out1 = nullptr, Variable* out2 = nullptr, Variable* out3 = nullptr);

    // Getters
    std::weak_ptr<BasicBlock> get_current_block() const { return current_block; }
    bool is_const_evaluatable() const { return constant_evaluatable; }

    void constant_evaluate();

    void print(std::ostream&) const;
};

class CFCOperation
{
    private:
    CFCInstruction cfc_instruction;
    std::array<std::shared_ptr<Variable>, 4> in_vars;
    std::weak_ptr<BasicBlock> current_block;

    // optional array of jump / call targets, useful for predecessor / successor identification
    std::vector<std::weak_ptr<BasicBlock>> targets;

    public:
    CFCOperation(CFCInstruction instr, std::shared_ptr<BasicBlock> bb, std::shared_ptr<Variable> op1 = nullptr,
     std::shared_ptr<Variable> op2 = nullptr, std::shared_ptr<Variable> op3 = nullptr, std::shared_ptr<Variable> op4 = nullptr)
    : cfc_instruction(instr),
      in_vars({op1, op2, op3, op4}),
      current_block(std::move(bb)),
      targets() { }

    void add_new_target(std::shared_ptr<BasicBlock> &);

    // Getters
    CFCInstruction get_cfc_instruction() const { return cfc_instruction; };
    const std::array<std::shared_ptr<Variable>, 4> &get_input_vars() const { return in_vars; };
    std::weak_ptr<BasicBlock> get_current_block() const { return current_block; }
};