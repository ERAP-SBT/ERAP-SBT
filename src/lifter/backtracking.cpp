#include <lifter/lifter.h>

using namespace lifter::RV64;

std::optional<uint64_t> Lifter::backtrace_jmp_addr(CfOp *op, BasicBlock *bb) {
    if (op->type != CFCInstruction::ijump) {
        std::cerr << "Jump address backtracking is currently only supported for indirect, JALR jumps." << std::endl;
        return std::nullopt;
    }
    std::vector<SSAVar *> parsed;
    return get_var_value(op->in_vars[0], bb, parsed);
}

std::optional<SSAVar *> Lifter::get_last_static_assignment(size_t idx, BasicBlock *bb) {
    std::vector<SSAVar *> possible_preds;

    // tuple<predecessor, desired target block, depth>
    std::vector<std::tuple<BasicBlock *, BasicBlock *, size_t>> unparsed_preds;
    for (BasicBlock *pred : bb->predecessors) {
        unparsed_preds.emplace_back(pred, bb, 0);
    }

    // store the already parsed ids
    std::vector<size_t> parsed_preds;
    parsed_preds.push_back(bb->id);

    while (!unparsed_preds.empty()) {
        BasicBlock *pred = std::get<0>(unparsed_preds.front());
        BasicBlock *des_target = std::get<1>(unparsed_preds.front());
        size_t pred_depth = std::get<2>(unparsed_preds.front());
        auto is_parsed = [pred](size_t b_id) -> bool { return b_id == pred->id; };

        if (pred_depth <= MAX_ADDRESS_SEARCH_DEPTH && std::find_if(parsed_preds.begin(), parsed_preds.end(), is_parsed) == parsed_preds.end()) {
            parsed_preds.push_back(pred->id);
            for (BasicBlock *pred_pred : pred->predecessors) {
                unparsed_preds.emplace_back(pred_pred, pred, pred_depth + 1);
            }

            for (auto &cfOp : pred->control_flow_ops) {
                if (cfOp.target() == des_target) {
                    // syscalls place their result in registers x10 and x11 and therefore invalidate the variables in these registers.
                    if (cfOp.type != CFCInstruction::syscall || (idx != 10 && idx != 11)) {
                        for (RefPtr<SSAVar> ti : cfOp.target_inputs()) {
                            if (!std::holds_alternative<size_t>(ti->info) && std::get<SSAVar::LifterInfo>(ti->lifter_info).static_id == idx) {
                                possible_preds.emplace_back(ti.release());
                            }
                        }
                    }
                }
            }
        }
        unparsed_preds.erase(unparsed_preds.begin());
    }
    if (possible_preds.empty()) {
        DEBUG_LOG("No predecessors for static variable were found in predecessor list of basic block.");
        return std::nullopt;
    } else if (possible_preds.size() > 1) {
        DEBUG_LOG("Warning: found multiple possible statically mapped variables. Selecting latest.");
    }
    return possible_preds.at(0);
}

void Lifter::load_input_vars(BasicBlock *bb, Operation *op, std::vector<int64_t> &resolved_vars, std::vector<SSAVar *> &parsed_vars) {
    for (auto in_var : op->in_vars) {
        if (in_var != nullptr) {
            // infinity recursion protection: only request the value of vars which weren't already requested.
            bool not_visited = std::find(parsed_vars.begin(), parsed_vars.end(), in_var) == parsed_vars.end();
            parsed_vars.push_back(in_var);

            if (not_visited) {
                auto res = get_var_value(in_var, bb, parsed_vars);
                if (res.has_value()) {
                    resolved_vars.push_back(res.value());
                }
            }
        }
    }
}

std::optional<int64_t> Lifter::get_var_value(SSAVar *var, BasicBlock *bb, std::vector<SSAVar *> &parsed_vars) {
    if (std::holds_alternative<size_t>(var->info)) {
        auto opt_var = get_last_static_assignment(std::get<SSAVar::LifterInfo>(var->lifter_info).static_id, bb);
        if (opt_var.has_value()) {
            var = opt_var.value();
        } else {
            return std::nullopt;
        }
    }
    if (std::holds_alternative<SSAVar::ImmInfo>(var->info)) {
        return std::get<SSAVar::ImmInfo>(var->info).val;
    }

    if (!std::holds_alternative<std::unique_ptr<Operation>>(var->info)) {
        return std::nullopt;
    }

    Operation *op = std::get<std::unique_ptr<Operation>>(var->info).get();
    std::vector<int64_t> resolved_vars;

    switch (op->type) {
    case Instruction::add:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] + resolved_vars[1];
    case Instruction::sub:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] - resolved_vars[1];
    case Instruction::immediate:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 1)
            return std::nullopt;
        return resolved_vars[0];
    case Instruction::shl:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] << resolved_vars[1];
    case Instruction::_or:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] | resolved_vars[1];
    case Instruction::_and:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] & resolved_vars[1];
    case Instruction::_not:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 1)
            return std::nullopt;
        return ~resolved_vars[0];
    case Instruction::_xor:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] ^ resolved_vars[1];
    case Instruction::sign_extend:
        // currently, only 32-bit to 64-bit sign extension is supported
        if (op->in_vars[0]->type != Type::i32 || var->type != Type::i64) {
            return std::nullopt;
        }
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 1)
            return std::nullopt;
        return (int64_t)resolved_vars[0];
    case Instruction::cast:
        load_input_vars(bb, op, resolved_vars, parsed_vars);
        if (resolved_vars.size() != 1)
            return std::nullopt;
        switch (op->out_vars[0]->type) {
        case Type::i64:
            return resolved_vars[0];
        case Type::i32:
            return (int32_t)resolved_vars[0];
        case Type::i16:
            return (int16_t)resolved_vars[0];
        case Type::i8:
            return (int8_t)resolved_vars[0];
        default:
            return std::nullopt;
        }
    default:
        std::stringstream str;
        str << "Warning: Jump target address is can't be calculated (unsupported operation): " << op->type;
        DEBUG_LOG(str.str());
        return std::nullopt;
    }
}
