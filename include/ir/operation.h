#pragma once

#include "instruction.h"
#include "variable.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

// forward declaration
struct BasicBlock;

struct Operation {
    Instruction type;
    std::array<RefPtr<SSAVar>, 4> in_vars = {};
    std::array<SSAVar *, 3> out_vars = {};

    // TODO: do we need that here?
    bool const_evaluable = false;

    explicit Operation(const Instruction type) : type(type) {}

    void set_inputs(SSAVar *in1 = nullptr, SSAVar *in2 = nullptr, SSAVar *in3 = nullptr, SSAVar *in4 = nullptr);
    void set_outputs(SSAVar *out1 = nullptr, SSAVar *out2 = nullptr, SSAVar *out3 = nullptr);

    void print(std::ostream &, const IR *) const;
};

struct CfOp {

    struct JumpInfo {
        BasicBlock *target = nullptr;
        std::vector<RefPtr<SSAVar>> target_inputs = {};
    };

    struct IJumpInfo {
        // jump addr is in in_vars[0]
        std::vector<std::pair<RefPtr<SSAVar>, size_t>> mapping = {};

        // we are sometimes able to infer the target (normally this means the ijump is a `ret`)
        BasicBlock *target = nullptr;
    };

    struct CJumpInfo {
        // Jump types: equals, !equals, unsigned less then, unsigned greater than, signed less than, signed greater than
        enum class CJumpType { eq, neq, lt, gt, slt, sgt };
        CJumpType type = CJumpType::eq;
        BasicBlock *target = nullptr;
        std::vector<RefPtr<SSAVar>> target_inputs = {};

        // only prints type
        void print(std::ostream &stream) const;
    };

    struct CallInfo {
        BasicBlock *continuation_block = nullptr;
        std::vector<std::pair<RefPtr<SSAVar>, size_t>> continuation_mapping = {}; // TODO: allow non-statics?
        BasicBlock *target = nullptr;
        std::vector<RefPtr<SSAVar>> target_inputs = {};
    };

    struct ICallInfo {
        // call addr is in in_vars[0]
        BasicBlock *continuation_block = nullptr;
        std::vector<std::pair<RefPtr<SSAVar>, size_t>> continuation_mapping = {}; // TODO: allow non-statics?
        std::vector<std::pair<RefPtr<SSAVar>, size_t>> mapping = {};
    };

    struct RetInfo {
        // SSAVar -> static
        std::vector<std::pair<RefPtr<SSAVar>, size_t>> mapping = {};
    };

    struct SyscallInfo {
        BasicBlock *continuation_block;
        std::vector<std::pair<RefPtr<SSAVar>, size_t>> continuation_mapping = {}; // TODO: allow non-statics?
        std::vector<size_t> static_mapping;                                       // output targets of syscall results
    };

    struct LifterInfo {
        // the virtual address to jump to, if known
        // just a reference for the lifter!
        uint64_t jump_addr;
        uint64_t instr_addr;
    };

    CFCInstruction type = CFCInstruction::unreachable;
    BasicBlock *source = nullptr;
    std::array<RefPtr<SSAVar>, 7> in_vars = {}; // 7 since syscall takes id + 6 args max
    std::variant<std::monostate, CJumpInfo, RetInfo, JumpInfo, IJumpInfo, CallInfo, ICallInfo, SyscallInfo> info;

    // std::vector<SSAVar *> target_inputs;
    std::variant<std::monostate, LifterInfo> lifter_info;

    // TODO: add info for const_evalness here? may be able to optimize control flow this way

    CfOp(CFCInstruction type, BasicBlock *source, BasicBlock *target);

    void set_inputs(SSAVar *op1 = nullptr, SSAVar *op2 = nullptr, SSAVar *op3 = nullptr, SSAVar *op4 = nullptr, SSAVar *op5 = nullptr, SSAVar *op6 = nullptr, SSAVar *op7 = nullptr);

    // these exist for the generators convinience atm, may be deleted later
    // the lifter currently depends on this method
    void add_target_input(SSAVar *input, size_t static_idx);

    void set_target(BasicBlock *target);

    BasicBlock *target() const;
    const std::vector<RefPtr<SSAVar>> &target_inputs() const;

    void print(std::ostream &, const IR *) const;
};
