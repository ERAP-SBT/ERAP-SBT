#pragma once

#include "instruction.h"
#include "variable.h"

#include <array>
#include <memory>
#include <vector>

// forward declaration
struct BasicBlock;

struct Operation {
    Instruction type;
    std::array<SSAVar *, 4> in_vars = {};
    std::array<SSAVar *, 3> out_vars = {};

    // TODO: do we need that here?
    bool const_evaluable = false;

    explicit Operation(const Instruction type) : type(type) {}
    ~Operation();

    void set_inputs(SSAVar *in1 = nullptr, SSAVar *in2 = nullptr, SSAVar *in3 = nullptr, SSAVar *in4 = nullptr);
    void set_outputs(SSAVar *out1 = nullptr, SSAVar *out2 = nullptr, SSAVar *out3 = nullptr);

    void print(std::ostream &, const IR *) const;
};

struct CfOp {
    struct CJumpInfo {
        enum class CJumpType { eq, neq, lt };
        CJumpType type;
        void print(std::ostream &stream) const;
    };

    CFCInstruction type;
    BasicBlock *source;
    BasicBlock *target;
    std::vector<SSAVar *> target_inputs;
    std::array<SSAVar *, 4> in_vars;
    std::variant<std::monostate, CJumpInfo> info;

    // TODO: add info for const_evalness here? may be able to optimize control flow this way

    CfOp(CFCInstruction type, BasicBlock *source, BasicBlock *target);

    void add_target_input(SSAVar *var) { target_inputs.push_back(var); }

    void set_inputs(SSAVar *op1 = nullptr, SSAVar *op2 = nullptr, SSAVar *op3 = nullptr, SSAVar *op4 = nullptr);

    void print(std::ostream &, const IR *) const;
};
