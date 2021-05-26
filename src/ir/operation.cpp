#include "ir/operation.h"

#include "ir/basic_block.h"
#include "ir/ir.h"

#include <cassert>

Operation::~Operation() {
    for (auto *var : in_vars) {
        if (var)
            var->ref_count--;
    }
}

void Operation::set_inputs(SSAVar *in1, SSAVar *in2, SSAVar *in3, SSAVar *in4) {
    assert(in_vars[0] == nullptr && in_vars[1] == nullptr && in_vars[2] == nullptr && in_vars[3] == nullptr);
    in_vars[0] = in1;
    in_vars[1] = in2;
    in_vars[2] = in3;
    in_vars[3] = in4;

    const_evaluable = true;
    for (auto *var : in_vars) {
        if (var) {
            var->ref_count++;
            if (!var->const_evaluable)
                const_evaluable = false;
        }
    }
}

void Operation::set_outputs(SSAVar *out1, SSAVar *out2, SSAVar *out3) {
    assert(out_vars[0] == nullptr && out_vars[1] == nullptr && out_vars[2] == nullptr);
    out_vars[0] = out1;
    out_vars[1] = out2;
    out_vars[2] = out3;
}

void Operation::print(std::ostream &stream, const IR *ir) const {
    stream << type << " ";

    bool first = true;
    for (auto *in : in_vars) {
        if (in) {
            if (!first)
                stream << ", ";
            else
                first = false;
            in->print_type_name(stream, ir);
        }
    }
}

CfOp::JumpInfo::~JumpInfo() {
    for (auto *var : _target_inputs) {
        var->ref_count--;
    }
}

CfOp::JumpInfo::JumpInfo(const JumpInfo &other) {
    target = other.target;
    _target_inputs = other._target_inputs;
    for (auto *var : _target_inputs) {
        var->ref_count++;
    }
}

CfOp::JumpInfo &CfOp::JumpInfo::operator=(const JumpInfo &other) {
    if (this != &other) {
        for (auto *var : _target_inputs) {
            var->ref_count--;
        }

        target = other.target;
        _target_inputs = other._target_inputs;

        for (auto *var : _target_inputs) {
            var->ref_count++;
        }
    }
    return *this;
}

CfOp::IJumpInfo::IJumpInfo(const IJumpInfo &other) {
    _mapping = other._mapping;
    for (auto &[var, s] : _mapping) {
        var->ref_count++;
    }
}

CfOp::IJumpInfo::~IJumpInfo() {
    for (auto &[var, s] : _mapping) {
        var->ref_count--;
    }
}

CfOp::IJumpInfo &CfOp::IJumpInfo::operator=(const IJumpInfo &other) {
    if (this != &other) {
        for (auto &[var, s] : _mapping) {
            var->ref_count--;
        }

        _mapping = other._mapping;

        for (auto &[var, s] : _mapping) {
            var->ref_count++;
        }
    }
    return *this;
}

CfOp::CfOp(const CFCInstruction type, BasicBlock *source, BasicBlock *target) : type(type), source(source), in_vars() {
    if (target != nullptr) {
        target->predecessors.push_back(source);
        source->successors.push_back(target);
    }

    switch (type) {
    case CFCInstruction::jump:
        assert(target != nullptr);
        info = JumpInfo{};
        std::get<JumpInfo>(info).target = target;
        break;
    case CFCInstruction::ijump:
        assert(target == nullptr);
        info = IJumpInfo{};
        break;
    case CFCInstruction::cjump:
        assert(target != nullptr);
        info = CJumpInfo{};
        std::get<CJumpInfo>(info).target = target;
        break;
    case CFCInstruction::call:
        assert(target != nullptr);
        info = CallInfo{};
        std::get<CallInfo>(info).target = target;
        break;
    case CFCInstruction::icall:
        assert(target == nullptr);
        info = ICallInfo{};
        break;
    case CFCInstruction::_return:
        assert(target == nullptr);
        info = RetInfo{};
        break;
    case CFCInstruction::unreachable:
        assert(target == nullptr);
        break;
    case CFCInstruction::syscall:
        assert(target != nullptr);
        info = SyscallInfo{};
        std::get<SyscallInfo>(info).continuation_block = target;
        break;
    }
}

CfOp::CfOp(const CfOp &other) {
    type = other.type;
    source = other.source;
    in_vars = other.in_vars;
    info = other.info;
    for (auto *var : in_vars) {
        if (var) {
            var->ref_count++;
        }
    }
}

CfOp::~CfOp() {
    for (auto *var : in_vars) {
        if (var) {
            var->ref_count--;
        }
    }
}

CfOp &CfOp::operator=(const CfOp &other) {
    if (this != &other) {
        for (auto *var : in_vars) {
            if (var) {
                var->ref_count--;
            }
        }

        type = other.type;
        source = other.source;
        in_vars = other.in_vars;
        info = other.info;

        for (auto *var : in_vars) {
            if (var) {
                var->ref_count++;
            }
        }
    }
    return *this;
}

void CfOp::set_inputs(SSAVar *op1, SSAVar *op2, SSAVar *op3, SSAVar *op4, SSAVar *op5, SSAVar *op6, SSAVar *op7) {
    assert(in_vars[0] == nullptr && in_vars[1] == nullptr && in_vars[2] == nullptr && in_vars[3] == nullptr && in_vars[4] == nullptr && in_vars[5] == nullptr && in_vars[6] == nullptr);
    in_vars[0] = op1;
    in_vars[1] = op2;
    in_vars[2] = op3;
    in_vars[3] = op4;
    in_vars[4] = op5;
    in_vars[5] = op6;
    in_vars[6] = op7;

    for (auto *var : in_vars) {
        if (var) {
            var->ref_count++;
        }
    }
}

void CfOp::add_target_input(SSAVar *input) {
    assert(type == CFCInstruction::jump || type == CFCInstruction::cjump || type == CFCInstruction::call);

    switch (type) {
    case CFCInstruction::jump:
        std::get<JumpInfo>(info).add_target_input(input);
        break;
    case CFCInstruction::cjump:
        std::get<CJumpInfo>(info).add_target_input(input);
        break;
    case CFCInstruction::call:
        std::get<CallInfo>(info).add_target_input(input);
        break;
    case CFCInstruction::syscall:
    case CFCInstruction::ijump:
    case CFCInstruction::icall:
    case CFCInstruction::unreachable:
    case CFCInstruction::_return:
        assert(0);
        break;
    }
}

BasicBlock *CfOp::target() const {
    // assert(type == CFCInstruction::jump || type == CFCInstruction::cjump || type == CFCInstruction::call || type == CFCInstruction::syscall);
    switch (type) {
    case CFCInstruction::jump:
        return std::get<JumpInfo>(info).target;
    case CFCInstruction::cjump:
        return std::get<CJumpInfo>(info).target;
    case CFCInstruction::call:
        return std::get<CallInfo>(info).target;
    case CFCInstruction::syscall:
        return std::get<SyscallInfo>(info).continuation_block;
    case CFCInstruction::ijump:
    case CFCInstruction::icall:
    case CFCInstruction::unreachable:
    case CFCInstruction::_return:
        return nullptr;
    }

    assert(0);
    return nullptr;
}

const std::vector<SSAVar *> &CfOp::target_inputs() const {
    assert(type == CFCInstruction::jump || type == CFCInstruction::cjump || type == CFCInstruction::call);
    static auto vec = std::vector<SSAVar *>{};

    switch (type) {
    case CFCInstruction::jump:
        return std::get<JumpInfo>(info).target_inputs();
    case CFCInstruction::cjump:
        return std::get<CJumpInfo>(info).target_inputs();
    case CFCInstruction::call:
        return std::get<CallInfo>(info).target_inputs();
    case CFCInstruction::syscall:
    case CFCInstruction::ijump:
    case CFCInstruction::icall:
    case CFCInstruction::unreachable:
    case CFCInstruction::_return:
        assert(0);
        return vec;
    }

    assert(0);
    return vec;
}

void CfOp::print(std::ostream &stream, const IR *ir) const {
    stream << '(' << type << ", [";
    if (type == CFCInstruction::_return) {
        auto first = true;
        const auto &ret_info = std::get<RetInfo>(info);
        for (const auto &[var, s_idx] : ret_info.mapping()) {
            if (!first) {
                stream << ", ";
            } else {
                first = false;
            }

            stream << ir->statics[s_idx].type << " @" << ir->statics[s_idx].id << " <- ";
            var->print_type_name(stream, ir);
        }

        stream << "])";
        return;
    }

    const auto *target = this->target();
    if (target) {
        target->print_name(stream, ir);

        if (type != CFCInstruction::syscall) {
            for (const auto *var : target_inputs()) {
                stream << ", ";
                var->print_type_name(stream, ir);
            }
        }
    }
    stream << "]";

    for (const auto *var : in_vars) {
        if (!var)
            continue;

        stream << ", ";
        var->print_type_name(stream, ir);
    }

    if (info.index() == 1) {
        // TODO
        stream << ", ";
        switch (info.index()) {
        case 1:
            std::get<CfOp::CJumpInfo>(info).print(stream);
            break;
        }
    }

    stream << ')';
}

CfOp::CJumpInfo::CJumpInfo(const CJumpInfo &other) {
    type = other.type;
    target = other.target;
    _target_inputs = other._target_inputs;
    for (auto *var : _target_inputs) {
        var->ref_count++;
    }
}

CfOp::CJumpInfo::~CJumpInfo() {
    for (auto *var : _target_inputs) {
        var->ref_count--;
    }
}

CfOp::CJumpInfo &CfOp::CJumpInfo::operator=(const CJumpInfo &other) {
    if (this != &other) {
        for (auto *var : _target_inputs) {
            var->ref_count--;
        }

        type = other.type;
        target = other.target;
        _target_inputs = other._target_inputs;

        for (auto *var : _target_inputs) {
            var->ref_count++;
        }
    }
    return *this;
}

void CfOp::CJumpInfo::print(std::ostream &stream) const {
    switch (type) {
    case CJumpInfo::CJumpType::eq:
        stream << "eq";
        break;
    case CJumpInfo::CJumpType::neq:
        stream << "neq";
        break;
    case CJumpInfo::CJumpType::lt:
        stream << "lt";
        break;
    }
}

CfOp::CallInfo::CallInfo(const CallInfo &other) {
    continuation_block = other.continuation_block;
    target = other.target;
    _target_inputs = other._target_inputs;
    for (auto *var : _target_inputs) {
        var->ref_count++;
    }
}

CfOp::CallInfo::~CallInfo() {
    for (auto *var : _target_inputs) {
        var->ref_count--;
    }
}

CfOp::CallInfo &CfOp::CallInfo::operator=(const CallInfo &other) {
    if (this != &other) {
        for (auto *var : _target_inputs) {
            var->ref_count--;
        }

        continuation_block = other.continuation_block;
        target = other.target;
        _target_inputs = other._target_inputs;

        for (auto *var : _target_inputs) {
            var->ref_count++;
        }
    }
    return *this;
}

CfOp::ICallInfo::ICallInfo(const ICallInfo &other) {
    continuation_block = other.continuation_block;
    _mapping = other._mapping;
    for (auto &[var, s] : _mapping) {
        var->ref_count++;
    }
}

CfOp::ICallInfo::~ICallInfo() {
    for (auto &[var, s] : _mapping) {
        var->ref_count--;
    }
}

CfOp::ICallInfo &CfOp::ICallInfo::operator=(const ICallInfo &other) {
    if (this != &other) {
        for (auto &[var, s] : _mapping) {
            var->ref_count--;
        }

        continuation_block = other.continuation_block;
        _mapping = other._mapping;

        for (auto &[var, s] : _mapping) {
            var->ref_count++;
        }
    }
    return *this;
}

CfOp::RetInfo::RetInfo(const RetInfo &other) {
    _mapping = other._mapping;
    for (auto &[var, s] : _mapping) {
        var->ref_count++;
    }
}

CfOp::RetInfo::~RetInfo() {
    for (auto &[var, s] : _mapping) {
        var->ref_count--;
    }
}

CfOp::RetInfo &CfOp::RetInfo::operator=(const RetInfo &other) {
    if (this != &other) {
        for (auto &[var, s] : _mapping) {
            var->ref_count--;
        }

        _mapping = other._mapping;

        for (auto &[var, s] : _mapping) {
            var->ref_count++;
        }
    }
    return *this;
}
