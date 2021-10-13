#include <lifter/lifter.h>

using namespace lifter::RV64;

std::unordered_set<int64_t> Lifter::backtrace_jmp_addrs(CfOp *op, BasicBlock *bb) {
    if (op->type != CFCInstruction::ijump && op->type != CFCInstruction::icall) {
        std::cerr << "Jump address backtracking is currently only supported for indirect jumps / calls." << std::endl;
        return {};
    }
    std::vector<SSAVar *> parsed;
    return get_var_values({op->in_vars[0]}, bb, parsed);
}

std::unordered_set<SSAVar *> Lifter::get_last_static_assignments(size_t idx, BasicBlock *bb) {
    // using a set here for performant searching
    std::unordered_set<SSAVar *> possible_preds;

    // tuple<predecessor, desired target block, depth>
    std::vector<std::tuple<BasicBlock *, BasicBlock *, size_t>> unparsed_preds;
    for (BasicBlock *pred : bb->predecessors) {
        unparsed_preds.emplace_back(pred, bb, 0);
    }

    // store the already parsed ids (set for searching)
    std::unordered_set<size_t> parsed_preds;
    parsed_preds.emplace(bb->id);

    size_t index = 0;

    while (index < unparsed_preds.size()) {
        BasicBlock *pred = std::get<0>(unparsed_preds[index]);
        BasicBlock *des_target = std::get<1>(unparsed_preds[index]);
        size_t pred_depth = std::get<2>(unparsed_preds[index]);

        if (pred_depth <= MAX_ADDRESS_SEARCH_DEPTH && parsed_preds.find(pred->id) == parsed_preds.end()) {
            parsed_preds.emplace(pred->id);
            for (BasicBlock *pred_pred : pred->predecessors) {
                unparsed_preds.emplace_back(pred_pred, pred, pred_depth + 1);
            }

            for (const auto &cfOp : pred->control_flow_ops) {
                if (cfOp.type == CFCInstruction::ijump) {
                    auto info = std::get<CfOp::IJumpInfo>(cfOp.info);
                    if (std::find(info.targets.begin(), info.targets.end(), des_target) != info.targets.end()) {
                        // search possible predecessors in target mapping
                        for (auto &input_pair : info.mapping) {
                            if (!std::holds_alternative<size_t>(input_pair.first->info) && input_pair.first->lifter_info().static_id == idx &&
                                possible_preds.find(input_pair.first.get()) == possible_preds.end()) {
                                possible_preds.emplace(input_pair.first.release());
                                if (!FULL_BACKTRACKING) {
                                    break;
                                }
                            }
                        }
                    }
                } else if (cfOp.target() == des_target) {
                    // syscalls place their result in registers x10 and x11 and therefore invalidate the variables in these registers.
                    if (cfOp.type != CFCInstruction::syscall || (idx != 10 && idx != 11)) {
                        for (SSAVar *ti : cfOp.target_inputs()) {
                            // The target input is a possible predecessor if it is not a static variable and it has the correct static index
                            if (!ti->is_static() && ti->lifter_info().static_id == idx && possible_preds.find(ti) == possible_preds.end()) {
                                possible_preds.emplace(ti);
                                if (!FULL_BACKTRACKING) {
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!FULL_BACKTRACKING && !possible_preds.empty()) {
                    break;
                }
            }
        }
        if (!FULL_BACKTRACKING && !possible_preds.empty()) {
            break;
        }
        ++index;
    }
    if (possible_preds.empty()) {
        DEBUG_LOG("No predecessors for static variable were found in predecessor list of basic block.");
    }

    if (FULL_BACKTRACKING || possible_preds.empty()) {
        return possible_preds;
    }
    return {*possible_preds.begin()};
}

std::vector<std::array<int64_t, 4>> Lifter::load_input_vars(BasicBlock *bb, Operation *op, std::vector<SSAVar *> &parsed_vars) {
    std::array<std::vector<int64_t>, 4> in_var_possibilities;
    for (size_t i = 0; i < op->in_vars.size(); i++) {
        if (op->in_vars[i] != nullptr) {
            // infinity recursion protection: only request the value of vars which weren't already requested.
            bool not_visited = std::find(parsed_vars.begin(), parsed_vars.end(), op->in_vars[i]) == parsed_vars.end();
            parsed_vars.push_back(op->in_vars[i]);

            if (not_visited) {
                for (auto res : get_var_values({op->in_vars[i]}, bb, parsed_vars)) {
                    in_var_possibilities[i].emplace_back(res);
                }
                if (in_var_possibilities[i].empty()) { // we always fill input variable slots from left to right -> missing left values make the statement un-backtrace-able
                    break;
                }
            }
        }
    }

    // This loop detects "holes" in the resolved in_var set. All variables right next to a hole (a variable without possible values) may not have values,
    // otherwise we can't compute the result of the operation afterwards. (only required variables are evaluated and thus we can assume that this would fail afterwards)
    size_t first_empty_index = SIZE_MAX;
    for (size_t i = 0; i < in_var_possibilities.size(); i++) {
        if (first_empty_index != SIZE_MAX && !in_var_possibilities[i].empty()) {
            return {};
        }
        if (first_empty_index == SIZE_MAX && in_var_possibilities[i].empty()) {
            first_empty_index = i;
        }
    }

    // The following code is designed to generate all possible combinations of input variables to the given operation. This might produce some impossible configurations but should also cover all
    // possible configurations.
    std::vector<std::array<int64_t, 4>> resolved_var_combinations;

    for (int64_t i0 : in_var_possibilities[0]) {
        resolved_var_combinations.push_back({i0, 0, 0, 0});
    }
    if (in_var_possibilities[1].empty()) {
        return resolved_var_combinations;
    }

    // remember the elements which were filled in the first iteration, we need to remove then later on
    size_t last_batch_size = resolved_var_combinations.size();

    for (int64_t i1 : in_var_possibilities[1]) {
        for (size_t i = 0; i < last_batch_size; i++) {
            // for each previously added combination, add one with each possible second input. We add these new configurations after the old ones into the same vector.
            resolved_var_combinations.push_back({resolved_var_combinations[i][0], i1, 0, 0});
        }
    }

    // remove all old configurations which are missing the second variable
    resolved_var_combinations.erase(resolved_var_combinations.begin(), std::next(resolved_var_combinations.begin(), (int)last_batch_size));
    last_batch_size = resolved_var_combinations.size();

    if (in_var_possibilities[2].empty()) {
        return resolved_var_combinations;
    }

    for (int64_t i2 : in_var_possibilities[2]) {
        for (size_t i = 0; i < last_batch_size; i++) {
            resolved_var_combinations.push_back({resolved_var_combinations[i][0], resolved_var_combinations[i][1], i2, 0});
        }
    }

    resolved_var_combinations.erase(resolved_var_combinations.begin(), std::next(resolved_var_combinations.begin(), (int)last_batch_size));
    last_batch_size = resolved_var_combinations.size();

    if (in_var_possibilities[3].empty()) {
        return resolved_var_combinations;
    }

    for (int64_t i3 : in_var_possibilities[3]) {
        for (size_t i = 0; i < last_batch_size; i++) {
            resolved_var_combinations.push_back({resolved_var_combinations[i][0], resolved_var_combinations[i][1], resolved_var_combinations[i][2], i3});
        }
    }
    return resolved_var_combinations;
}

std::unordered_set<int64_t> Lifter::get_var_values(const std::vector<SSAVar *> &start_vars, BasicBlock *bb, std::vector<SSAVar *> &parsed_vars) {
    std::unordered_set<int64_t> return_values;
    for (auto start_var : start_vars) {
        std::unordered_set<SSAVar *> backtrack_vars = {start_var};

        if (std::holds_alternative<size_t>(start_var->info)) {
            backtrack_vars = get_last_static_assignments(start_var->lifter_info().static_id, bb);
            if (backtrack_vars.empty()) {
                DEBUG_LOG("Couldn't resolve static variable via backtracking. Skipping this branch.");
                continue;
            }
        }

        for (auto back_var : backtrack_vars) {
            // immediates can be resolved immediately
            if (back_var->is_immediate()) {
                return_values.emplace(back_var->get_immediate().val);
                continue;
            }

            // There shouldn't be any static variables at this point, if there are any, we need to skip them.
            if (!back_var->is_operation()) {
                DEBUG_LOG("Only operation result variables are expected to exist at this point. Backtracking continues, but you might miss jump targets.");
                continue;
            }

            Operation *op = &back_var->get_operation();

            std::vector<std::array<int64_t, 4>> resolved_vars_combs;

            // these functions are just here to avoid code duplication
            std::function<int64_t(int64_t, int64_t)> op2;
            std::function<int64_t(int64_t)> op1;

            switch (op->type) {
            case Instruction::add:
                op2 = std::plus<>();
                break;
            case Instruction::sub:
                op2 = std::minus<>();
                break;
            case Instruction::shl:
                op2 = [](int64_t x1, int64_t x2) { return x1 << x2; }; // c++20: std::shift_left
                break;
            case Instruction::_or:
                op2 = std::bit_or<>();
                break;
            case Instruction::_and:
                op2 = std::bit_and<>();
                break;
            case Instruction::_not:
                op1 = std::bit_not<>();
                break;
            case Instruction::_xor:
                op2 = std::bit_xor<>();
                break;
            case Instruction::sign_extend:
                // currently, only 32-bit to 64-bit sign extension is supported
                if (op->in_vars[0]->type != Type::i32 || back_var->type != Type::i64) {
                    DEBUG_LOG("Encountered unsupported sign extension during backtracking. Only 32-Bit to 64-Bit integer sign extension is currently supported.");
                    continue;
                }
                resolved_vars_combs = load_input_vars(bb, op, parsed_vars);
                for (std::array<int64_t, 4> combination : resolved_vars_combs) {
                    return_values.emplace((int64_t)(int32_t)combination[0]);
                }
                continue;
            case Instruction::cast:
                resolved_vars_combs = load_input_vars(bb, op, parsed_vars);
                switch (op->out_vars[0]->type) {
                case Type::i64:
                    for (std::array<int64_t, 4> combination : resolved_vars_combs) {
                        return_values.emplace(combination[0]);
                    }
                    continue;
                case Type::i32:
                    for (std::array<int64_t, 4> combination : resolved_vars_combs) {
                        return_values.emplace((int32_t)combination[0]);
                    }
                    continue;
                case Type::i16:
                    for (std::array<int64_t, 4> combination : resolved_vars_combs) {
                        return_values.emplace((int16_t)combination[0]);
                    }
                    continue;
                case Type::i8:
                    for (std::array<int64_t, 4> combination : resolved_vars_combs) {
                        return_values.emplace((int8_t)combination[0]);
                    }
                    continue;
                default:
                    DEBUG_LOG("Invalid source type for casting, skipping cast.");
                    continue;
                }
            default:
                // TODO: handle store and load operations
                std::stringstream str;
                str << "Warning: Jump target address can't be calculated (unsupported operation): " << op->type;
                DEBUG_LOG(str.str());
                continue;
            }

            resolved_vars_combs = load_input_vars(bb, op, parsed_vars);
            if (op1 != nullptr) {
                for (std::array<int64_t, 4> combination : resolved_vars_combs) {
                    return_values.emplace(op1(combination[0]));
                }
            } else if (op2 != nullptr) {
                for (std::array<int64_t, 4> combination : resolved_vars_combs) {
                    return_values.emplace(op2(combination[0], combination[1]));
                }
            }
        }
    }
    return return_values;
}
