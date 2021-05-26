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

    // TODO: in theory you could skip all that protected jumbo here and just make sure that
    // TODO: CfOp::add_target_input does the correct ref counting stuff
    struct JumpInfo {
        BasicBlock *target = nullptr;

      protected:
        std::vector<SSAVar *> _target_inputs = {};

      public:
        ~JumpInfo();
        JumpInfo() = default;
        JumpInfo(const JumpInfo &);

        void add_target_input(SSAVar *var) {
            var->ref_count++;
            _target_inputs.push_back(var);
        }

        std::vector<SSAVar *> &target_inputs() { return _target_inputs; }

        const std::vector<SSAVar *> &target_inputs() const { return _target_inputs; }

        JumpInfo &operator=(const JumpInfo &);
    };

    struct IJumpInfo {
        // jump addr is in in_vars[0]
      protected:
        std::vector<std::pair<SSAVar *, size_t>> _mapping = {};

      public:
        IJumpInfo() = default;
        IJumpInfo(const IJumpInfo &);
        ~IJumpInfo();

        IJumpInfo &operator=(const IJumpInfo &);

        void add_mapping(SSAVar *var, const size_t static_idx) {
            var->ref_count++;
            _mapping.emplace_back(var, static_idx);
        }

        std::vector<std::pair<SSAVar *, size_t>> &mapping() { return _mapping; }

        const std::vector<std::pair<SSAVar *, size_t>> &mapping() const { return _mapping; }
    };

    struct CJumpInfo {
        enum class CJumpType { eq, neq, lt };
        CJumpType type = CJumpType::eq;
        BasicBlock *target = nullptr;

      protected:
        std::vector<SSAVar *> _target_inputs = {};

      public:
        CJumpInfo() = default;
        CJumpInfo(const CJumpInfo &);
        ~CJumpInfo();

        CJumpInfo &operator=(const CJumpInfo &);

        void add_target_input(SSAVar *var) {
            var->ref_count++;
            _target_inputs.push_back(var);
        }

        std::vector<SSAVar *> &target_inputs() { return _target_inputs; }

        const std::vector<SSAVar *> &target_inputs() const { return _target_inputs; }

        // only prints type
        void print(std::ostream &stream) const;
    };

    struct CallInfo {
        BasicBlock *continuation_block = nullptr; // may not have non-static inputs
        BasicBlock *target = nullptr;

      protected:
        std::vector<SSAVar *> _target_inputs = {};

      public:
        CallInfo() = default;
        CallInfo(const CallInfo &);
        ~CallInfo();

        CallInfo &operator=(const CallInfo &);

        void add_target_input(SSAVar *var) {
            var->ref_count++;
            _target_inputs.push_back(var);
        }

        std::vector<SSAVar *> &target_inputs() { return _target_inputs; }

        const std::vector<SSAVar *> &target_inputs() const { return _target_inputs; }
    };

    struct ICallInfo {
        // call addr is in in_vars[0]
        BasicBlock *continuation_block = nullptr; // may not have non-static inputs
      protected:
        std::vector<std::pair<SSAVar *, size_t>> _mapping = {};

      public:
        ICallInfo() = default;
        ICallInfo(const ICallInfo &);
        ~ICallInfo();

        ICallInfo &operator=(const ICallInfo &);

        void add_mapping(SSAVar *var, const size_t static_idx) {
            var->ref_count++;
            _mapping.emplace_back(var, static_idx);
        }

        std::vector<std::pair<SSAVar *, size_t>> &mapping() { return _mapping; }

        const std::vector<std::pair<SSAVar *, size_t>> &mapping() const { return _mapping; }
    };

    struct RetInfo {
      protected:
        // SSAVar -> static
        std::vector<std::pair<SSAVar *, size_t>> _mapping = {};

      public:
        RetInfo() = default;
        RetInfo(const RetInfo &);
        ~RetInfo();

        RetInfo &operator=(const RetInfo &);

        void add_mapping(SSAVar *var, const size_t static_idx) {
            var->ref_count++;
            _mapping.emplace_back(var, static_idx);
        }

        std::vector<std::pair<SSAVar *, size_t>> &mapping() { return _mapping; }

        const std::vector<std::pair<SSAVar *, size_t>> &mapping() const { return _mapping; }
    };

    struct SyscallInfo {
        BasicBlock *continuation_block;       // allow non-static inputs here?
        std::optional<size_t> static_mapping; // output target of syscall result
    };

    CFCInstruction type = CFCInstruction::unreachable;
    BasicBlock *source = nullptr;
    std::array<SSAVar *, 7> in_vars = {}; // 7 since syscall takes id + 6 args max
    std::variant<std::monostate, CJumpInfo, RetInfo, JumpInfo, IJumpInfo, CallInfo, ICallInfo, SyscallInfo> info;

    // TODO: add info for const_evalness here? may be able to optimize control flow this way

    CfOp(CFCInstruction type, BasicBlock *source, BasicBlock *target);
    CfOp(const CfOp &);
    ~CfOp();

    CfOp &operator=(const CfOp &);

    void set_inputs(SSAVar *op1 = nullptr, SSAVar *op2 = nullptr, SSAVar *op3 = nullptr, SSAVar *op4 = nullptr, SSAVar *op5 = nullptr, SSAVar *op6 = nullptr, SSAVar *op7 = nullptr);

    // these exist for the generators convinience atm, may be deleted later
    // usage in the lifter shouldn't be needed
    void add_target_input(SSAVar *input);

    BasicBlock *target() const;
    const std::vector<SSAVar *> &target_inputs() const;

    void print(std::ostream &, const IR *) const;
};
