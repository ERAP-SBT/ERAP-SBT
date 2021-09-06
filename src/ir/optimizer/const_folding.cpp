#include "ir/optimizer/const_folding.h"

#include "ir/eval.h"
#include "ir/instruction.h"
#include "ir/ir.h"
#include "ir/type.h"

#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <utility>

namespace optimizer {

namespace {

constexpr bool is_unary_op(Instruction insn) { return insn == Instruction::_not; }
constexpr bool is_binary_op(Instruction insn) {
    switch (insn) {
    case Instruction::add:
    case Instruction::sub:
    case Instruction::mul_l:
    case Instruction::ssmul_h:
    case Instruction::uumul_h:
    case Instruction::sumul_h:
    case Instruction::shl:
    case Instruction::shr:
    case Instruction::sar:
    case Instruction::_or:
    case Instruction::_and:
    case Instruction::_xor:
        return true;
    default:
        return false;
    }
}
constexpr bool is_morphing_op(Instruction insn) { return insn == Instruction::sign_extend || insn == Instruction::zero_extend || insn == Instruction::cast; }

Type resolve_simple_op_type(const Operation &op) {
    // TODO This set is only here to help find possible errors and should be removed.
    std::set<Type> type_set;

    for (const auto &in : op.in_vars) {
        if (in && in->type != Type::imm) {
            type_set.insert(in->type);
        }
    }
    for (const auto &out : op.out_vars) {
        if (out && out->type != Type::imm) {
            type_set.insert(out->type);
        }
    }
    if (type_set.empty()) {
        std::cerr << "Warning: Could not resolve definitive non-imm type\n";
        return Type::i64;
    } else if (type_set.size() > 1) {
        std::cerr << "Warning: Type conflict detected! Types are:\n";
        Type largest = Type::i8;
        for (Type t : type_set) {
            std::cerr << " - " << t << '\n';
            if (cast_dir(largest, t) == 1) {
                largest = t;
            }
        }
        return largest;
    } else {
        return *type_set.begin();
    }
}

class ConstFoldPass {
    std::map<size_t, SSAVar *> rewrites;
    std::vector<std::unique_ptr<SSAVar>> new_immediates;
    BasicBlock *current_block;

  public:
    void process_block(BasicBlock *block);

    SSAVar *queue_imm(Type type, uint64_t imm, bool bin_rel = false);

    void rewrite_visit_refptr(RefPtr<SSAVar> &);
    void do_input_rewrites(Operation &);
    void do_cf_rewrites(CfOp &);

    void replace_with_immediate(SSAVar *var, uint64_t immediate, bool bin_rel = false);
    void replace_var(SSAVar *var, SSAVar *new_var);

    /** Simplify commutative operation. Used by simplify_bi_imm_{left,right} */
    void simplify_bi_comm(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in);
    /** Simplify a binary operation with an immediate on the left (e.g. v2 <- add 0, v1) */
    void simplify_bi_imm_left(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in);
    /** Simplify a binary operation with an immediate on the right (e.g. v2 <- add v1, 0) */
    void simplify_bi_imm_right(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in);

    /** Simplify a morphing operation (e.g. i32 v2 <- cast i32 v1) */
    void simplify_morph(Instruction insn, Type in_type, SSAVar *in_var, Type out_type, SSAVar *out_var);

    /** Simplify an operation with an immediate on the left and an operation on the right */
    void simplify_double_op_imm_left(Type type, Operation &cur, Operation &prev);
    /** Simplify an operation with an immediate on the right and an operation on the left */
    void simplify_double_op_imm_right(Type type, Operation &cur, Operation &prev);

    void fixup_block();
};

SSAVar *ConstFoldPass::queue_imm(Type type, uint64_t imm, bool bin_rel) {
    size_t new_id = current_block->cur_ssa_id++;
    auto new_var = std::make_unique<SSAVar>(new_id, imm, bin_rel);
    new_var->type = type;
    return new_immediates.emplace_back(std::move(new_var)).get();
}

// Note: Prefix notation in comments means IR operation, infix notation means operation on / evaluation of immediates.

// TODO needs thorough tests

void ConstFoldPass::simplify_double_op_imm_left(Type type, Operation &cur, Operation &prev) {
    // ca: imm, cb: operation
    auto &ca = cur.in_vars[0], &cb = cur.in_vars[1];
    auto &cai = ca->get_immediate();
    if (cur.type == Instruction::add) {
        if (prev.type == Instruction::add) {
            auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
            assert(!pa->is_immediate() || !pb->is_immediate()); // Ops with imm/imm should be folded by now
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // add ca, (add pa, pb)
                // => add (pa + ca), pb
                auto result = eval_binary_op(Instruction::add, type, pa->get_immediate().val, cai.val);
                ca = queue_imm(type, result);
                cb = pb;
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // add imm, (add pa, pb)
                // => add (pb + ca), pa
                auto result = eval_binary_op(Instruction::add, type, pb->get_immediate().val, cai.val);
                ca = queue_imm(type, result);
                cb = pa;
            }
        } else if (prev.type == Instruction::sub) {
            auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
            assert(!pa->is_immediate() || !pb->is_immediate());
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // add ca, (sub pa, pb)
                // => sub (pa + ca), pb
                auto result = eval_binary_op(Instruction::add, type, pa->get_immediate().val, cai.val);
                cur.type = Instruction::sub;
                ca = queue_imm(type, result);
                cb = pb;
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // add ca, (sub pa, pb)
                // => add (ca - pb), pa
                auto result = eval_binary_op(Instruction::sub, type, cai.val, pb->get_immediate().val);
                ca = queue_imm(type, result);
                cb = pa;
            }
        }
    } else if (cur.type == Instruction::sub) {
        if (prev.type == Instruction::add) {
            auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
            assert(!pa->is_immediate() || !pb->is_immediate());
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // sub ca, (add pa, pb)
                // => sub (ca - pa), pb
                auto result = eval_binary_op(Instruction::sub, type, cai.val, pa->get_immediate().val);
                ca = queue_imm(type, result);
                cb = pb;
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // sub ca, (add pa, pb)
                // => sub (ca - pb), pa
                auto result = eval_binary_op(Instruction::sub, type, cai.val, pb->get_immediate().val);
                ca = queue_imm(type, result);
                cb = pa;
            }
        } else if (prev.type == Instruction::sub) {
            auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
            assert(!pa->is_immediate() || !pb->is_immediate());
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // sub ca, (sub pa, pb)
                // => add (ca - pa), pb
                auto result = eval_binary_op(Instruction::sub, type, cai.val, pa->get_immediate().val);
                cur.type = Instruction::add;
                ca = queue_imm(type, result);
                cb = pb;
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // sub ca, (sub pa, pb)
                // => sub (pb + ca), pa
                auto result = eval_binary_op(Instruction::add, type, pb->get_immediate().val, cai.val);
                ca = queue_imm(type, result);
                cb = pa;
            }
        }
    }
}

void ConstFoldPass::simplify_double_op_imm_right(Type type, Operation &cur, Operation &prev) {
    // ca: operation, cb: imm
    auto &ca = cur.in_vars[0], &cb = cur.in_vars[1];
    auto &cbi = cb->get_immediate();
    if (cur.type == Instruction::add) {
        if (prev.type == Instruction::add) {
            auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
            assert(!pa->is_immediate() || !pb->is_immediate());
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // add (add pa, pb), cb
                // => add (pa + cb), pb
                auto result = eval_binary_op(Instruction::add, type, pa->get_immediate().val, cbi.val);
                ca = queue_imm(type, result);
                cb = pb;
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // add (add pa, pb), cb
                // => add (pb + cb), pa
                auto result = eval_binary_op(Instruction::add, type, pb->get_immediate().val, cbi.val);
                ca = queue_imm(type, result);
                cb = pa;
            }
        } else if (prev.type == Instruction::sub) {
            auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
            assert(!pa->is_immediate() || !pb->is_immediate());
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // add (sub pa, pb), cb
                // => sub (pa + cb), pb
                auto result = eval_binary_op(Instruction::add, type, pa->get_immediate().val, cbi.val);
                cur.type = Instruction::sub;
                ca = queue_imm(type, result);
                cb = pb;
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // add (sub pa, pb), cb
                // => sub pa, (pb - cb)
                auto result = eval_binary_op(Instruction::sub, type, pb->get_immediate().val, cbi.val);
                cur.type = Instruction::sub;
                ca = pa;
                cb = queue_imm(type, result);
            }
        }
    } else if (cur.type == Instruction::sub) {
        if (prev.type == Instruction::add) {
            auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
            assert(!pa->is_immediate() || !pb->is_immediate());
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // sub (add pa, pb), cb
                // => add (pa - cb), pb
                auto result = eval_binary_op(Instruction::sub, type, pa->get_immediate().val, cbi.val);
                cur.type = Instruction::add;
                ca = queue_imm(type, result);
                cb = pb;
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // sub (add pa, pb), cb
                // => add (pb - cb), pa
                auto result = eval_binary_op(Instruction::sub, type, pb->get_immediate().val, cbi.val);
                cur.type = Instruction::add;
                ca = queue_imm(type, result);
                cb = pa;
            }
        } else if (prev.type == Instruction::sub) {
            auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
            assert(!pa->is_immediate() || !pb->is_immediate());
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // sub (sub pa, pb), cb
                // => sub (pa - cb), pb
                auto result = eval_binary_op(Instruction::sub, type, pa->get_immediate().val, cbi.val);
                ca = queue_imm(type, result);
                cb = pb;
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // sub (sub pa, pb), cb
                // => sub pa, (pb - cb)
                auto result = eval_binary_op(Instruction::sub, type, pb->get_immediate().val, cbi.val);
                ca = pa;
                cb = queue_imm(type, result);
            }
        }
    }
}

void ConstFoldPass::replace_with_immediate(SSAVar *var, uint64_t imm, bool bin_rel) { var->info = SSAVar::ImmInfo{static_cast<int64_t>(imm), bin_rel}; }

void ConstFoldPass::replace_var(SSAVar *var, SSAVar *new_var) { rewrites[var->id] = new_var; }

void ConstFoldPass::simplify_bi_comm(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in) {
    switch (insn) {
    case Instruction::add:
        if (immediate == 0) {
            replace_var(cur, in);
        }
        break;
    case Instruction::_and:
        if (typed_equal(type, immediate, UINT64_MAX)) {
            replace_var(cur, in);
        } else if (typed_equal(type, immediate, 0)) {
            replace_with_immediate(cur, 0);
        }
        break;
    case Instruction::_or:
        if (typed_equal(type, immediate, UINT64_MAX)) {
            replace_with_immediate(cur, typed_narrow(type, UINT64_MAX));
        } else if (typed_equal(type, immediate, 0)) {
            replace_var(cur, in);
        }
        break;
    case Instruction::_xor:
        if (typed_equal(type, immediate, 0)) {
            replace_var(cur, in);
        }
        break;
    default:
        break;
    }
}

constexpr bool is_commutative(Instruction insn) {
    switch (insn) {
    case Instruction::add:
    case Instruction::_and:
    case Instruction::_or:
    case Instruction::_xor:
        return true;
    default:
        return false;
    }
}

void ConstFoldPass::simplify_bi_imm_left(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in) {
    if (is_commutative(insn)) {
        simplify_bi_comm(insn, type, immediate, cur, in);
        return;
    }
    switch (insn) {
    case Instruction::shl:
    case Instruction::shr:
    case Instruction::sar:
        if (typed_equal(type, immediate, 0)) {
            replace_with_immediate(cur, 0);
        }
        break;
    default:
        break;
    }
}

void ConstFoldPass::simplify_bi_imm_right(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in) {
    if (is_commutative(insn)) {
        simplify_bi_comm(insn, type, immediate, cur, in);
        return;
    }
    switch (insn) {
    case Instruction::sub:
        if (typed_equal(type, immediate, 0)) {
            replace_var(cur, in);
        }
        break;
    case Instruction::shl:
    case Instruction::shr:
    case Instruction::sar:
        if (typed_equal(type, immediate, 0)) {
            replace_var(cur, in);
        }
        break;
    default:
        break;
    }
}

void ConstFoldPass::simplify_morph(Instruction insn, Type in_type, SSAVar *in_var, Type out_type, SSAVar *out_var) {
    assert(insn == Instruction::sign_extend || insn == Instruction::zero_extend || insn == Instruction::cast);

    if (in_type == out_type) {
        replace_var(out_var, in_var);
    }
}

void ConstFoldPass::rewrite_visit_refptr(RefPtr<SSAVar> &ref) {
    if (!ref)
        return;
    auto it = rewrites.find(ref->id);
    if (it != rewrites.end()) {
        ref.reset(it->second);
    }
}

void ConstFoldPass::do_input_rewrites(Operation &op) {
    for (auto &in_var : op.in_vars) {
        rewrite_visit_refptr(in_var);
    }
}

template <typename> [[maybe_unused]] inline constexpr bool always_false_v = false;

void ConstFoldPass::do_cf_rewrites(CfOp &cf) {
    for (auto &in : cf.in_vars) {
        rewrite_visit_refptr(in);
    }
    std::visit(
        [this](auto &info) {
            using T = std::decay_t<decltype(info)>;
            if constexpr (std::is_same_v<T, CfOp::JumpInfo> || std::is_same_v<T, CfOp::CJumpInfo>) {
                for (auto &var : info.target_inputs) {
                    rewrite_visit_refptr(var);
                }
            } else if constexpr (std::is_same_v<T, CfOp::IJumpInfo> || std::is_same_v<T, CfOp::RetInfo>) {
                for (auto &var : info.mapping) {
                    rewrite_visit_refptr(var.first);
                }
            } else if constexpr (std::is_same_v<T, CfOp::CallInfo>) {
                for (auto &var : info.continuation_mapping) {
                    rewrite_visit_refptr(var.first);
                }
                for (auto &var : info.target_inputs) {
                    rewrite_visit_refptr(var);
                }
            } else if constexpr (std::is_same_v<T, CfOp::ICallInfo>) {
                for (auto &var : info.continuation_mapping) {
                    rewrite_visit_refptr(var.first);
                }
                for (auto &var : info.mapping) {
                    rewrite_visit_refptr(var.first);
                }
            } else if constexpr (std::is_same_v<T, CfOp::SyscallInfo>) {
                for (auto &var : info.continuation_mapping) {
                    rewrite_visit_refptr(var.first);
                }
            } else if constexpr (!std::is_same_v<T, std::monostate>) {
                static_assert(always_false_v<T>, "Missing CfOp info in rewrite visitor");
            }
        },
        cf.info);
}

constexpr bool can_handle_types(std::initializer_list<Type> types) {
    for (auto type : types) {
        if (!is_integer(type) && type != Type::imm)
            return false;
    }
    return true;
}

void ConstFoldPass::process_block(BasicBlock *block) {
    rewrites.clear();
    current_block = block;

    for (size_t var_index = 0; var_index < block->variables.size(); var_index++) {
        auto &var = block->variables[var_index];
        if (!var->is_operation())
            continue;

        auto &op = var->get_operation();
        do_input_rewrites(op);

        if (is_binary_op(op.type)) {
            auto &a = op.in_vars[0], &b = op.in_vars[1];
            if (!can_handle_types({a->type, b->type}))
                continue;
            Type type = resolve_simple_op_type(op);
            if (a->is_immediate() && b->is_immediate()) {
                // op imm, imm
                auto &ia = a->get_immediate(), &ib = b->get_immediate();

                // Evaluating binary-relative immediates only makes sense in three cases:
                // `a + (bin_rel b)`, `(bin_rel a) + b` and `(bin_rel a) - b`.
                if (ia.binary_relative && ib.binary_relative)
                    continue;
                bool bin_rel = ia.binary_relative || ib.binary_relative;
                if (bin_rel && !(op.type == Instruction::add || op.type == Instruction::sub))
                    continue;
                if (bin_rel) {
                    if (op.type == Instruction::sub) {
                        // Evaluating `a - (bin_rel b)` doesn't make sense
                        // (simplifies to a - b - base, which still requires the sub operation).
                        if (ib.binary_relative)
                            continue;
                    } else if (op.type != Instruction::add) {
                        continue;
                    }
                }

                int64_t result = eval_binary_op(op.type, type, ia.val, ib.val);
                replace_with_immediate(var.get(), result, bin_rel);
                continue;
            } else {
                if (a->is_immediate() && b->is_operation()) {
                    if (a->get_immediate().binary_relative)
                        continue; // TODO
                    // op imm, op
                    simplify_double_op_imm_left(type, op, b->get_operation());
                } else if (b->is_immediate() && a->is_operation()) {
                    if (b->get_immediate().binary_relative)
                        continue; // TODO
                    // op op, imm
                    simplify_double_op_imm_right(type, op, a->get_operation());
                }

                // simplify_double_op_* can change the order parameters; thus the separate check
                if (a->is_immediate()) {
                    const auto &imm = a->get_immediate();
                    if (imm.binary_relative)
                        continue;
                    // op imm, any
                    simplify_bi_imm_left(op.type, type, imm.val, var.get(), b);
                } else if (b->is_immediate()) {
                    const auto &imm = b->get_immediate();
                    if (imm.binary_relative)
                        continue;
                    // op any, imm
                    simplify_bi_imm_right(op.type, type, imm.val, var.get(), a);
                }
            }
        } else if (is_unary_op(op.type)) {
            auto &in = op.in_vars[0];
            if (!can_handle_types({in->type}))
                continue;
            if (in->is_immediate()) {
                // op imm
                auto &ii = in->get_immediate();
                if (ii.binary_relative)
                    continue;
                Type type = resolve_simple_op_type(op);
                int64_t result = eval_unary_op(op.type, type, ii.val);
                replace_with_immediate(var.get(), result);
            } else if (in->is_operation()) {
                auto &prev = in->get_operation();
                if (prev.type == Instruction::_not) {
                    // not (not x) = x
                    replace_var(var.get(), prev.in_vars[0]);
                }
            }
        } else if (is_morphing_op(op.type)) {
            auto &in = op.in_vars[0];
            if (!can_handle_types({in->type}))
                continue;
            assert(op.out_vars[0] == var.get());
            Type input = in->type, output = var->type;
            if (in->is_immediate()) {
                // op imm
                auto &ii = in->get_immediate();
                if (input == Type::imm) {
                    std::cerr << "Found morphing operation (b" << block->id << "/v" << var->id << ") with imm as input, this shouldn't really happen\n";
                    input = output;
                }
                if (ii.binary_relative)
                    continue;
                int64_t result = eval_morphing_op(op.type, input, output, ii.val);
                replace_with_immediate(var.get(), result);
            } else {
                // op any
                simplify_morph(op.type, input, in, output, var.get());
            }
        } else if (op.type == Instruction::div || op.type == Instruction::udiv) {
            // TODO handle multiple outputs
            auto &a = op.in_vars[0], &b = op.in_vars[1];
            if (!can_handle_types({a->type, b->type}))
                continue;
            assert((op.out_vars[0] == nullptr) != (op.out_vars[1] == nullptr));
            if (a->is_immediate() && b->is_immediate()) {
                // div(u) imm, imm
                auto &ai = a->get_immediate(), &bi = b->get_immediate();
                if (ai.binary_relative || bi.binary_relative)
                    continue;
                Type type = resolve_simple_op_type(op);
                auto [div_result, rem_result] = eval_div(op.type, type, b->get_immediate().val, a->get_immediate().val);
                int64_t result = op.out_vars[0] ? div_result : rem_result;
                replace_with_immediate(var.get(), result);
            }
        }

        // New immediates can't be inserted directly, because that would invalidate the references above.
        // However, nothing prevents us from not inserting immediates until the end of the iteration.
        for (auto &new_imm : new_immediates) {
            auto insert_point = std::next(block->variables.begin(), var_index);
            block->variables.insert(insert_point, std::move(new_imm));
            ++var_index;
        }
        new_immediates.clear();
    }

    // Apply rewrites to control flow ops
    for (auto &cf : block->control_flow_ops) {
        do_cf_rewrites(cf);
    }

    fixup_block();
}

void ConstFoldPass::fixup_block() {
    // Currently, the generator fails if an immediate hasn't type Type::imm
    for (auto &var : current_block->variables) {
        if (var->is_immediate())
            var->type = Type::imm;
    }
}

} // namespace

void const_fold(IR *ir) {
    ConstFoldPass pass;
    for (auto &bb : ir->basic_blocks) {
        pass.process_block(bb.get());
    }
}

} // namespace optimizer
