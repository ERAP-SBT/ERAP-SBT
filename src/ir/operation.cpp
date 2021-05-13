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

void Operation::add_inputs(SSAVar *op1, SSAVar *op2, SSAVar *op3, SSAVar *op4)
{
    in_vars[0] = op1;
    in_vars[1] = op2;
    in_vars[2] = op3;
    in_vars[3] = op4;

    const_evalable = true;
    for (auto *var : in_vars)
    {
        if (var)
        {
            var->ref_count++;
            if (!var->const_evalable)
                const_evalable = false;
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
    switch (type)
    {
    case Instruction::add:
        stream << "add ";
        in_vars[0]->print_type_name(stream, ir);
        stream << ", ";
        in_vars[1]->print_type_name(stream, ir);
        break;
    default: stream << "unk"; break;
    }
}

CfOp::CfOp(const CFCInstruction type, BasicBlock *source, BasicBlock *target) : type(type), source(source), target(target)
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
    stream << '(';
    // TODO: move elsewhere
    switch (type)
    {
    case CFCInstruction::jump: stream << "jump"; break;
    case CFCInstruction::cjump: stream << "cjump"; break;
    default: stream << "unk";
    }

    stream << ", [";
    target->print_name(stream, ir);

    for (const auto *var : target_inputs)
    {
        stream << ", ";
        var->print_type_name(stream, ir);
    }
    stream << "]";

    switch (type)
    {
    case CFCInstruction::cjump:
    {
        stream << ", ";
        switch (std::get<1>(info).type)
        {
        case CJumpInfo::CJumpType::eq: stream << "eq"; break;
        case CJumpInfo::CJumpType::neq: stream << "neq"; break;
        case CJumpInfo::CJumpType::lt: stream << "lt"; break;
        }
        break;
    }
    default: break;
    }

    stream << ')';
}
