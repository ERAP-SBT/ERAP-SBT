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

    uint64_t virt_start_addr = 0;
    uint64_t virt_end_addr = 0;

    std::vector<CfOp> control_flow_ops;
    std::vector<BasicBlock *> predecessors;
    std::vector<BasicBlock *> successors;

    std::vector<SSAVar *> inputs;
    std::vector<std::unique_ptr<SSAVar>> variables;
    std::string dbg_name;

    BasicBlock(IR *ir, const size_t id, const size_t virt_start_addr = 0, const std::string &dbg_name = {}) : ir(ir), id(id), virt_start_addr(virt_start_addr), dbg_name(dbg_name) {}
    ~BasicBlock();

    SSAVar *add_var(const Type type, uint64_t assign_addr, size_t reg = 0) {
        auto var = std::make_unique<SSAVar>(cur_ssa_id++, type);
        var->lifter_info = SSAVar::LifterInfo{assign_addr, reg};
        const auto ptr = var.get();
        variables.push_back(std::move(var));
        return ptr;
    }

    SSAVar *add_var_imm(const int64_t imm, uint64_t assign_addr, const bool binary_relative = false, size_t reg = 0) {
        if (!assign_addr) {
            reg += 1;
        }
        auto var = std::make_unique<SSAVar>(cur_ssa_id++, imm, binary_relative);
        var->lifter_info = SSAVar::LifterInfo{assign_addr, reg};
        const auto ptr = var.get();
        variables.push_back(std::move(var));
        return ptr;
    }

    SSAVar *add_var_from_static(size_t static_idx, uint64_t assign_addr = 0);

    SSAVar *add_input(SSAVar *var) {
        inputs.emplace_back(var);
        return var;
    }

    CfOp &add_cf_op(CFCInstruction type, BasicBlock *target, uint64_t instr_addr = 0, uint64_t jump_addr = 0) { return add_cf_op(type, this, target, instr_addr, jump_addr); }

    CfOp &add_cf_op(CFCInstruction type, BasicBlock *source, BasicBlock *target, uint64_t instr_addr = 0, uint64_t jump_addr = 0) {
        CfOp &cf_op = control_flow_ops.emplace_back(type, source, target);
        cf_op.lifter_info = CfOp::LifterInfo{jump_addr, instr_addr};
        return control_flow_ops.back();
    }

    void print(std::ostream &, const IR *) const;
    void print_name(std::ostream &, const IR *) const;
};
