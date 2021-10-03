#include "ir/optimizer/dedup_imm.h"

#include "ir/optimizer/common.h"

#include <unordered_set>
#include <vector>

template <> struct std::hash<SSAVar::ImmInfo> {
    size_t operator()(const SSAVar::ImmInfo &key) const noexcept { return std::hash<uint64_t>()(key.val) ^ std::hash<bool>()(key.binary_relative); }
};

struct VarMeta {
    SSAVar *var;

    explicit VarMeta(SSAVar *v) : var(v) {}

    bool operator==(const VarMeta &other) const noexcept {
        SSAVar *ovar = other.var;
        if (var->type != ovar->type)
            return false;
        if (var->info.index() != ovar->info.index())
            return false;
        if (var->is_immediate()) {
            return var->get_immediate() == ovar->get_immediate();
        } else if (var->is_static()) {
            return var->get_static() == ovar->get_static();
        } else if (var->is_operation()) {
            const auto &op = var->get_operation(), &oop = ovar->get_operation();
            if (op.type != oop.type)
                return false;
            if (op.in_vars.size() != oop.in_vars.size() || op.out_vars.size() != oop.out_vars.size())
                return false; // Currently, in/out_vars are arrays, but this check future-proofs it.
            for (size_t i = 0; i < op.in_vars.size(); i++) {
                // Compare by id here, since all inputs must be declared before the variable
                if (!op.in_vars[i] && !oop.in_vars[i])
                    continue;
                if (!op.in_vars[i] || !oop.in_vars[i])
                    continue;
                if (op.in_vars[i]->id != oop.in_vars[i]->id)
                    return false;
            }
            for (size_t i = 0; i < op.out_vars.size(); i++) {
                // Only compare output position here
                if (!op.out_vars[i] != !oop.out_vars[i])
                    return false;
            }
            if (op.rounding_info != oop.rounding_info)
                return false;
            return true;
        } else {
            assert(0 && "Uninitialized IR variable in optimizer");
            return false;
        }
    }
};

template <> struct std::hash<VarMeta> {
    size_t operator()(const VarMeta &key) const noexcept {
        // Similar to operator==
        auto *var = key.var;
        if (var->is_immediate()) {
            return std::hash<uint32_t>()(static_cast<uint32_t>(var->type)) ^ std::hash<SSAVar::ImmInfo>()(var->get_immediate());
        } else if (var->is_static()) {
            return std::hash<uint32_t>()(static_cast<uint32_t>(var->type)) ^ std::hash<size_t>()(var->get_static());
        } else if (var->is_operation()) {
            const auto &op = var->get_operation();
            size_t hash = std::hash<uint32_t>()(static_cast<uint32_t>(op.type));
            for (auto &in_var : op.in_vars) {
                if (in_var) {
                    hash ^= std::hash<size_t>()(in_var->id);
                }
            }
            for (size_t i = 0; i < op.out_vars.size(); i++) {
                if (op.out_vars[i]) {
                    hash ^= std::hash<size_t>()(i);
                }
            }
            // TODO rounding mode
            return hash;
        } else {
            assert(0 && "Uninitialized IR variable in optimizer");
            return 0;
        }
    }
};

namespace optimizer {

void dedup_imm(IR *ir) {
    VarRewriter rw;
    std::unordered_set<VarMeta> vars;
    std::vector<size_t> deduplicated_indices;

    for (auto &bb : ir->basic_blocks) {
        rw.clear();
        vars.clear();
        deduplicated_indices.clear();

        for (size_t vi = 0; vi < bb->variables.size(); vi++) {
            auto *var = bb->variables[vi].get();
            if (var->is_operation()) {
                rw.apply_to(var->get_operation());

                auto insn = var->get_operation().type;
                if (insn == Instruction::store || insn == Instruction::load) {
                    continue;
                }
            }

            auto replacement = vars.find(VarMeta(var));
            if (replacement != vars.end()) {
                // The current variable is duplicated. Remove and replace.
                rw.replace(var, replacement->var);
                deduplicated_indices.push_back(vi);
            } else {
                vars.emplace(VarMeta(var));
            }
        }

        rw.apply_to(bb->control_flow_ops);

        for (auto it = deduplicated_indices.rbegin(), end = deduplicated_indices.rend(); it != end; ++it) {
            bb->variables.erase(std::next(bb->variables.begin(), *it));
        }
    }
}

} // namespace optimizer
