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

CfOp::CfOp(const CFCInstruction type, BasicBlock *source, BasicBlock *target) : type(type), source(source), target(target), in_vars() {
  if (target != nullptr) {
    target->predecessors.push_back(source);
    source->successors.push_back(target);
  }
}

void CfOp::set_inputs(SSAVar *op1, SSAVar *op2, SSAVar *op3, SSAVar *op4) {
    assert(in_vars[0] == nullptr && in_vars[1] == nullptr && in_vars[2] == nullptr && in_vars[3] == nullptr);
    in_vars[0] = op1;
    in_vars[1] = op2;
    in_vars[2] = op3;
    in_vars[3] = op4;

    for (auto *var : in_vars) {
        if (var) {
            var->ref_count++;
        }
    }
}

void CfOp::print(std::ostream &stream, const IR *ir) const {
    stream << '(' << type << ", [";
    if (target) {
      target->print_name(stream, ir);

    for (const auto *var : target_inputs) {
        stream << ", ";
        var->print_type_name(stream, ir);
      }
    }
    stream << "]";

    for (const auto *var : in_vars) {
        if (!var)
            continue;

        stream << ", ";
        var->print_type_name(stream, ir);
    }

    if (info.index() != 0) {
        stream << ", ";
        switch (info.index()) {
        case 1:
            std::get<CfOp::CJumpInfo>(info).print(stream);
            break;
        }
    }

    stream << ')';
}

void CfOp::CJumpInfo::print(std::ostream &stream) const {
    stream << ", ";
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
