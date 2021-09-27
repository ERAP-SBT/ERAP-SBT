#include "ir/basic_block.h"

#include "ir/ir.h"

#include <sstream>

BasicBlock::~BasicBlock() {
    control_flow_ops.clear();
    predecessors.clear();
    successors.clear();
    inputs.clear();
    while (!variables.empty()) {
        variables.pop_back();
    }
}

SSAVar *BasicBlock::add_var_from_static(const size_t static_idx, uint64_t assign_addr) {
    const auto &static_var = ir->statics[static_idx];
    auto var = std::make_unique<SSAVar>(cur_ssa_id++, static_var.type, static_idx);
    var->lifter_info = SSAVar::LifterInfo{assign_addr, static_idx};
    const auto ptr = var.get();
    variables.push_back(std::move(var));
    if (static_idx) {
        add_input(ptr);
    }
    return ptr;
}

void BasicBlock::set_virt_end_addr(uint64_t addr) {
    assert(addr != 0);

    this->virt_end_addr = addr;

    /* Update this->ir->virt_bb_ptrs */
    for (size_t i = (this->virt_start_addr - this->ir->virt_bb_start_addr) / 2; i <= (this->virt_end_addr - this->ir->virt_bb_start_addr) / 2; i++) {
        this->ir->virt_bb_ptrs[i] = this;
    }
}

static void verify_print_bb_name(const BasicBlock &bb, std::ostream &out) {
    out << "In basic block ";
    out << 'b' << bb.id;
    if (!bb.dbg_name.empty()) {
        out << " (" << bb.dbg_name << ')';
    }
    out << ": ";
}

bool BasicBlock::verify(std::vector<std::string> &messages_out) const {
    bool ok = true;
    std::unordered_set<decltype(SSAVar::id)> variable_ids;

    for (const auto &var : variables) {
        // The variable's id must not occur twice
        if (variable_ids.find(var->id) != variable_ids.end()) {
            std::stringstream s;
            verify_print_bb_name(*this, s);
            s << "Variable " << var->id << " is declared twice.";
            messages_out.push_back(s.str());
            ok = false;
        }

        if (auto op = std::get_if<std::unique_ptr<Operation>>(&var->info)) {
            for (const auto &param : (*op)->in_vars) {
                // Operation parameters must be declared before the operation itself
                if (param) {
                    if (variable_ids.find(param->id) == variable_ids.end()) {
                        std::stringstream s;
                        verify_print_bb_name(*this, s);
                        s << "Variable " << var->id << " has a dependency on variable " << param->id;
                        s << ", which has not been declared yet.";
                        messages_out.push_back(s.str());
                        ok = false;
                    }
                }
            }
        }

        variable_ids.insert(var->id);
    }

    for (const auto &cf_op : control_flow_ops) {
        if (cf_op.type == CFCInstruction::unreachable || cf_op.type == CFCInstruction::_return) {
            continue;
        }

        if (cf_op.type != CFCInstruction::ijump) {
            for (const auto &input : cf_op.target_inputs()) {
                // Control flow operations must only reference variables in the current basic block
                if (variable_ids.find(input->id) == variable_ids.end()) {
                    std::stringstream s;
                    verify_print_bb_name(*this, s);
                    s << "Control flow operation with type " << cf_op.type << " references variable " << input->id;
                    s << ", which is not declared in this basic block.";
                    messages_out.push_back(s.str());
                    ok = false;
                }
            }
        } else {
            for (const auto &input : std::get<CfOp::IJumpInfo>(cf_op.info).mapping) {
                // Control flow operations must only reference variables in the current basic block
                if (variable_ids.find(input.first->id) == variable_ids.end()) {
                    std::stringstream s;
                    verify_print_bb_name(*this, s);
                    s << "Control flow operation with type " << cf_op.type << " references variable " << input.first->id;
                    s << ", which is not declared in this basic block.";
                    messages_out.push_back(s.str());
                    ok = false;
                }
            }
        }

        auto *target = cf_op.target();
        if (target) {
            // targets needs this bb in its predecessor list
            if (std::find(target->predecessors.begin(), target->predecessors.end(), this) == target->predecessors.end()) {
                std::stringstream s;
                verify_print_bb_name(*this, s);
                s << "Target " << target->id << " of CfOp with type " << cf_op.type << " does not have this BB in its predecessor list.";
                messages_out.push_back(s.str());
                ok = false;
            }

            // target needs to be in successor list
            if (std::find(successors.begin(), successors.end(), target) == successors.end()) {
                std::stringstream s;
                verify_print_bb_name(*this, s);
                s << "Target " << target->id << " of CfOp with type " << cf_op.type << " is not in Successor-List of this BB.";
                messages_out.push_back(s.str());
                ok = false;
            }
        }
    }

    // each predecessor needs to have a cfop with this block as its target
    for (auto *predecessor : predecessors) {
        auto has_target = false;
        for (const auto &cf_op : predecessor->control_flow_ops) {
            if (cf_op.target() == this) {
                has_target = true;
                break;
            }
            if (cf_op.type == CFCInstruction::call) {
                if (std::get<CfOp::CallInfo>(cf_op.info).continuation_block == this) {
                    has_target = true;
                    break;
                }
            } else if (cf_op.type == CFCInstruction::icall) {
                if (std::get<CfOp::ICallInfo>(cf_op.info).continuation_block == this) {
                    has_target = true;
                    break;
                }
            }
        }
        if (!has_target) {
            std::stringstream s;
            verify_print_bb_name(*this, s);
            s << "BB " << predecessor->id << " is marked as a predecessor of this BB but has no CfOp with this block as its target.";
            messages_out.push_back(s.str());
            ok = false;
        }
    }

    // each successor needs to have a cfop in this block with it as a target
    for (auto *successor : successors) {
        auto has_target = false;
        for (const auto &cf_op : control_flow_ops) {
            if (cf_op.target() == successor) {
                has_target = true;
                break;
            }
            if (cf_op.type == CFCInstruction::call) {
                if (std::get<CfOp::CallInfo>(cf_op.info).continuation_block == successor) {
                    has_target = true;
                    break;
                }
            } else if (cf_op.type == CFCInstruction::icall) {
                if (std::get<CfOp::ICallInfo>(cf_op.info).continuation_block == successor) {
                    has_target = true;
                    break;
                }
            }
        }
        if (!has_target) {
            std::stringstream s;
            verify_print_bb_name(*this, s);
            s << "BB " << successor->id << " is marked as a successor of this BB but there is no CfOp with it as its target.";
            messages_out.push_back(s.str());
            ok = false;
        }
    }

    return ok;
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
    stream << "] {";
    if (!dbg_name.empty()) {
        stream << " // " << dbg_name;
    }
    stream << '\n';

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
