#pragma once

#include "operation.h"

#include <memory>
#include <optional>
#include <ostream>
#include <vector>

// forward declaration
struct IR;

struct BasicBlock {
    IR *ir;
    size_t id;
    size_t cur_ssa_id = 0;
    uintptr_t offset = 0; // offset from base of image in orig binary

    std::vector<CfOp> control_flow_ops;
    std::vector<BasicBlock *> predecessors;
    std::vector<BasicBlock *> successors;

    std::vector<SSAVar *> inputs;
    std::vector<std::unique_ptr<SSAVar>> variables;

    BasicBlock(IR *ir, const size_t id, const size_t offset = 0) : ir(ir), id(id), offset(offset) {}
    ~BasicBlock();

    SSAVar *add_var(const Type type) {
        auto var = std::make_unique<SSAVar>(cur_ssa_id++, type);
        const auto ptr = var.get();
        variables.push_back(std::move(var));
        return ptr;
    }

    SSAVar *add_var_imm(const int64_t imm, const bool binary_relative = false) {
        auto var = std::make_unique<SSAVar>(cur_ssa_id++, imm, binary_relative);
        const auto ptr = var.get();
        variables.push_back(std::move(var));
        return ptr;
    }

    SSAVar *add_var_from_static(const size_t static_idx);

    SSAVar *add_input(SSAVar *var) {
        inputs.emplace_back(var);
        return var;
    }

    CfOp &add_cf_op(const CFCInstruction type, BasicBlock *target) {
        control_flow_ops.emplace_back(type, this, target);
        return control_flow_ops.back();
    }

    void print(std::ostream &, const IR *) const;
    void print_name(std::ostream &, const IR *) const;
};
