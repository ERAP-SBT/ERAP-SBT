#include "ir/optimizer/common.h"

namespace optimizer {

void VarRewriter::replace(SSAVar *old_var, SSAVar *new_var) { rewrites[old_var->id] = new_var; }

void VarRewriter::visit_refptr(RefPtr<SSAVar> &ref) const {
    if (!ref)
        return;
    auto it = rewrites.find(ref->id);
    if (it != rewrites.end()) {
        ref.reset(it->second);
    }
}

void VarRewriter::apply_to(Operation &op) const {
    for (auto &in_var : op.in_vars) {
        visit_refptr(in_var);
    }

    if (std::holds_alternative<RefPtr<SSAVar>>(op.rounding_info)) {
        visit_refptr(std::get<RefPtr<SSAVar>>(op.rounding_info));
    }
}

template <typename> [[maybe_unused]] inline constexpr bool always_false_v = false;

void VarRewriter::apply_to(CfOp &cf) const {
    for (auto &in : cf.in_vars) {
        visit_refptr(in);
    }
    std::visit(
        [this](auto &info) {
            using T = std::decay_t<decltype(info)>;
            if constexpr (std::is_same_v<T, CfOp::JumpInfo> || std::is_same_v<T, CfOp::CJumpInfo>) {
                for (auto &var : info.target_inputs) {
                    visit_refptr(var);
                }
            } else if constexpr (std::is_same_v<T, CfOp::IJumpInfo> || std::is_same_v<T, CfOp::RetInfo>) {
                for (auto &var : info.mapping) {
                    visit_refptr(var.first);
                }
            } else if constexpr (std::is_same_v<T, CfOp::CallInfo>) {
                for (auto &var : info.target_inputs) {
                    visit_refptr(var);
                }
            } else if constexpr (std::is_same_v<T, CfOp::ICallInfo>) {
                for (auto &var : info.mapping) {
                    visit_refptr(var.first);
                }
            } else if constexpr (std::is_same_v<T, CfOp::SyscallInfo>) {
                for (auto &var : info.continuation_mapping) {
                    visit_refptr(var.first);
                }
            } else if constexpr (!std::is_same_v<T, std::monostate>) {
                static_assert(always_false_v<T>, "Missing CfOp info in rewrite visitor");
            }
        },
        cf.info);
}

[[noreturn]] void panic_internal(const char *file, int line, const char *message) {
    fprintf(stderr, "Panicked at %s:%d: %s\n", file, line, message != nullptr ? message : "(no reason given)");
    std::abort();
}

} // namespace optimizer
