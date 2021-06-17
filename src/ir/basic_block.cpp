#include "ir/basic_block.h"

#include "ir/ir.h"

BasicBlock::~BasicBlock() {
    control_flow_ops.clear();
    predecessors.clear();
    successors.clear();
    inputs.clear();
    while (!variables.empty()) {
        variables.pop_back();
    }
}

SSAVar *BasicBlock::add_var_from_static(const size_t static_idx) {
    const auto &static_var = ir->statics[static_idx];
    auto var = std::make_unique<SSAVar>(cur_ssa_id++, static_var.type, static_idx);
    const auto ptr = var.get();
    variables.push_back(std::move(var));
    return ptr;
}

void BasicBlock::print(std::ostream &stream, const IR *ir) const {
    stream << "block b" << id << "(";
    // not very cool rn
    {
        auto first = true;
        for (const auto &var : inputs) {
            if (!first) {
                stream << ", ";
            } else {
                first = false;
            }
            var->print(stream, ir);
        }
    }
    stream << ") <= [";
    {
        auto first = true;
        for (const auto &pred : predecessors) {
            if (!first) {
                stream << ", ";
            } else {
                first = false;
            }
            pred->print_name(stream, ir);
        }
    }
    stream << "] {\n";

    for (const auto &var : variables) {
        stream << "  ";
        var->print(stream, ir);
        stream << '\n';
    }

    stream << "} => [";

    auto first = true;
    for (const auto &cf_op : control_flow_ops) {
        if (!first) {
            stream << ", ";
        } else {
            first = false;
        }

        cf_op.print(stream, ir);
    }
    stream << ']';
}

void BasicBlock::print_name(std::ostream &stream, const IR *) const { stream << "b" << id; }
