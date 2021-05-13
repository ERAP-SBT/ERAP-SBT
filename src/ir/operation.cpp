#include "ir/operation.h"

#include "ir/basic_block.h"

Operation::~Operation()
{
    for (auto *var : in_vars)
    {
        if (var)
            var->ref_count--;
    }
}

void Operation::add_inputs(SSAVar *in1, SSAVar *in2, SSAVar *in3, SSAVar *in4)
{
    in_vars[0] = in1;
    in_vars[1] = in2;
    in_vars[2] = in3;
    in_vars[3] = in4;

    const_evaluable = true;
    for (auto *var : in_vars)
    {
        if (var)
        {
            var->ref_count++;
            if (!var->const_evaluable)
                const_evaluable = false;
        }
    }
}

void Operation::add_outputs(SSAVar *out1, SSAVar *out2, SSAVar *out3)
{
    out_vars[0] = out1;
    out_vars[1] = out2;
    out_vars[2] = out3;
}

void Operation::print(std::ostream &stream, const IR *ir) const
{
    stream << type << " ";

    bool first = true;
    for (auto *in : in_vars)
    {
        if (in)
        {
            if (!first)
                stream << ", ";
            else
                first = false;
            in->print_type_name(stream, ir);
        }
    }
}

CfOp::CfOp(const CFCInstruction type, BasicBlock *source, BasicBlock *target) : type(type), source(source), target(target), in_vars()
{
    target->predecessors.push_back(source);
    source->successors.push_back(target);
}

void CfOp::add_inputs(SSAVar *op1, SSAVar *op2, SSAVar *op3, SSAVar *op4)
{
    in_vars[0] = op1;
    in_vars[1] = op2;
    in_vars[2] = op3;
    in_vars[3] = op4;

    for (auto *var : in_vars)
    {
        if (var)
        {
            var->ref_count++;
        }
    }
}

void CfOp::print(std::ostream &stream, const IR *ir) const
{
    stream << '(' << type << ", [";
    target->print_name(stream, ir);

    for (const auto *var : target_inputs)
    {
        stream << ", ";
        var->print_type_name(stream, ir);
    }
    stream << "]";

    if (info.index() != 0)
    {
        stream << ", ";
        switch (info.index())
        {
        case 1: std::get<CfOp::CJumpInfo>(info).print(stream); break;
        }
    }

    stream << ')';
}

void CfOp::CJumpInfo::print(std::ostream &stream) const
{
    stream << ", ";
    switch (type)
    {
    case CJumpInfo::CJumpType::eq: stream << "eq"; break;
    case CJumpInfo::CJumpType::neq: stream << "neq"; break;
    case CJumpInfo::CJumpType::lt: stream << "lt"; break;
    }
}