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
        std::cerr << "Warning: Type conflict detected\n";
        Type largest = Type::i8;
        for (Type t : type_set) {
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
    BasicBlock *current_block;
    size_t var_index;

  public:
    void process_block(BasicBlock *block);

    SSAVar *insert_imm_var(Type type, int64_t imm, bool bin_rel = false);

    void do_input_rewrites(Operation &op);

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

SSAVar *ConstFoldPass::insert_imm_var(Type type, int64_t imm, bool bin_rel) {
    size_t new_id = current_block->cur_ssa_id++;
    auto new_var = std::make_unique<SSAVar>(new_id, imm, bin_rel);
    new_var->type = type;
    auto insert_point = std::next(current_block->variables.begin(), var_index);
    ++var_index;
    return current_block->variables.insert(insert_point, std::move(new_var))->get();
}

void ConstFoldPass::simplify_double_op_imm_left(Type type, Operation &cur, Operation &prev) {
    auto &ca = cur.in_vars[0], &cb = cur.in_vars[1];
    if (cur.type == Instruction::add && prev.type == Instruction::add) {
        auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
        if (pa->is_immediate()) {
            assert(!pb->is_immediate());
            // v1 <- add imm, any (not imm)
            // v2 <- add imm, v1
            int64_t result = eval_binary_op(Instruction::add, type, pa->get_immediate().val, ca->get_immediate().val);
            ca = insert_imm_var(type, result);
            cb = pb;
        } else if (pb->is_immediate()) {
            assert(!pa->is_immediate());
            // v1 <- add any (not imm), imm
            // v2 <- add imm, v1
            int64_t result = eval_binary_op(Instruction::add, type, pb->get_immediate().val, ca->get_immediate().val);
            ca = insert_imm_var(type, result);
            cb = pa;
        }
    }
}

void ConstFoldPass::simplify_double_op_imm_right(Type type, Operation &cur, Operation &prev) {
    auto &ca = cur.in_vars[0], &cb = cur.in_vars[1];
    if (cur.type == Instruction::add && prev.type == Instruction::add) {
        auto &pa = prev.in_vars[0], &pb = prev.in_vars[1];
        if (pa->is_immediate()) {
            assert(!pb->is_immediate());
            // v1 <- add imm, any (not imm)
            // v2 <- add v1, imm
            int64_t result = eval_binary_op(Instruction::add, type, pa->get_immediate().val, cb->get_immediate().val);
            cb = insert_imm_var(type, result);
            ca = pb;
        } else if (pb->is_immediate()) {
            assert(!pa->is_immediate());
            // v1 <- add any (not imm), imm
            // v2 <- add v1, imm
            int64_t result = eval_binary_op(Instruction::add, type, pb->get_immediate().val, cb->get_immediate().val);
            cb = insert_imm_var(type, result);
            ca = pa;
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

void ConstFoldPass::do_input_rewrites(Operation &op) {
    for (auto &in_var : op.in_vars) {
        if (!in_var)
            continue;
        auto it = rewrites.find(in_var->id);
        if (it != rewrites.end()) {
            in_var.reset(it->second);
        }
    }
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

    for (var_index = 0; var_index < block->variables.size(); var_index++) {
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
            } else if (a->is_immediate()) {
                if (a->get_immediate().binary_relative)
                    continue; // TODO
                if (b->is_operation()) {
                    // op imm, op
                    simplify_double_op_imm_left(type, op, b->get_operation());
                }
                // op imm, any
                simplify_bi_imm_left(op.type, type, a->get_immediate().val, var.get(), b);
            } else if (b->is_immediate()) {
                if (b->get_immediate().binary_relative)
                    continue; // TODO
                if (a->is_operation()) {
                    // op op, imm
                    simplify_double_op_imm_right(type, op, a->get_operation());
                }
                // op any, imm
                simplify_bi_imm_right(op.type, type, b->get_immediate().val, var.get(), a);
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
                    std::cerr << "Found morphing operation with imm as input, this shouldn't really happen\n";
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
    }

    // Apply rewrites to control flow ops
    for (auto &cf : block->control_flow_ops) {
        for (auto &in : cf.in_vars) {
            if (!in)
                continue;
            auto it = rewrites.find(in->id);
            if (it != rewrites.end()) {
                in.reset(it->second);
            }
        }
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
