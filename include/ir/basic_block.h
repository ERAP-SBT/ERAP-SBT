#pragma once

#include "operation.h"

#include <memory>
#include <optional>
#include <ostream>
#include <vector>

// forward declaration
struct IR;

struct BasicBlock {
    IR *const ir;
    const size_t id;
    size_t cur_ssa_id = 0;

    /* Only the dummy basicblock should have an virt_start_addr=0 */
    const uint64_t virt_start_addr;
    uint64_t virt_end_addr = 0;

    std::vector<CfOp> control_flow_ops;
    std::vector<BasicBlock *> predecessors;
    std::vector<BasicBlock *> successors;

    std::vector<SSAVar *> inputs;
    std::vector<std::unique_ptr<SSAVar>> variables;
    const std::string dbg_name;

    struct GeneratorInfo {
        bool compiled = false;
        bool input_map_setup = false;
        // for circular-reference-chain
        bool manual_top_level = false;
        bool call_target = false;
        bool call_cont_block = false;
        size_t max_stack_size = 0;

        struct InputInfo {
            enum LOCATION { STATIC, REGISTER, STACK };

            LOCATION location;
            union {
                size_t reg_idx = 0;
                size_t stack_slot;
                size_t static_idx;
            };
        };
        std::vector<InputInfo> input_map;
    };
    GeneratorInfo gen_info;

    BasicBlock(IR *ir, const size_t id, const size_t virt_start_addr, std::string dbg_name = {}) : ir(ir), id(id), virt_start_addr{virt_start_addr}, dbg_name(std::move(dbg_name)) {}
    ~BasicBlock();

    SSAVar *add_var(const Type type, uint64_t assign_addr, size_t reg = SIZE_MAX) {
        auto var = std::make_unique<SSAVar>(cur_ssa_id++, type);
        var->lifter_info = SSAVar::LifterInfo{assign_addr, reg};
        const auto ptr = var.get();
        variables.push_back(std::move(var));
        return ptr;
    }

    SSAVar *add_var_imm(const int64_t imm, uint64_t assign_addr, const bool binary_relative = false, size_t reg = SIZE_MAX) {
        auto var = std::make_unique<SSAVar>(cur_ssa_id++, imm, binary_relative);
        var->lifter_info = SSAVar::LifterInfo{assign_addr, reg};
        const auto ptr = var.get();
        variables.push_back(std::move(var));
        return ptr;
    }

    SSAVar *add_var_from_static(size_t static_idx, uint64_t assign_addr = 0);

    /* NOTE: has side-effects on *ir, end_addr=0 is invalid */
    void set_virt_end_addr(uint64_t end_addr);

    SSAVar *add_input(SSAVar *var) {
        inputs.emplace_back(var);
        return var;
    }

    CfOp &add_cf_op(CFCInstruction type, BasicBlock *target, uint64_t instr_addr = 0, uint64_t jump_addr = 0) {
        CfOp &cf_op = control_flow_ops.emplace_back(type, this, target);
        cf_op.lifter_info = CfOp::LifterInfo{jump_addr, instr_addr};
        return control_flow_ops.back();
    }

    bool verify(std::vector<std::string> &messages_out) const;

    void print(std::ostream &, const IR *) const;
    void print_name(std::ostream &, const IR *) const;
};
