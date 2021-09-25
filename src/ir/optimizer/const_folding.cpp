#include "ir/optimizer/const_folding.h"

#include "ir/eval.h"
#include "ir/instruction.h"
#include "ir/ir.h"
#include "ir/optimizer/common.h"
#include "ir/type.h"

#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <utility>

namespace optimizer {

namespace {

constexpr bool one_of_rec(Instruction) { return false; }
template <typename... Set> constexpr bool one_of_rec(Instruction insn, Instruction next, Set &&...in) { return insn == next || one_of_rec(insn, in...); }
template <typename... Set> constexpr bool one_of(Instruction insn, Set &&...in) { return one_of_rec(insn, in...); }

constexpr bool is_unary_op(Instruction insn) { return one_of(insn, Instruction::_not); }
constexpr bool is_binary_op(Instruction insn) {
    return one_of(insn, Instruction::add, Instruction::sub, Instruction::mul_l, Instruction::ssmul_h, Instruction::uumul_h, Instruction::sumul_h, Instruction::shl, Instruction::shr, Instruction::sar,
                  Instruction::_or, Instruction::_and, Instruction::_xor, Instruction::max, Instruction::umax, Instruction::min, Instruction::umin);
}
constexpr bool is_morphing_op(Instruction insn) { return one_of(insn, Instruction::sign_extend, Instruction::zero_extend, Instruction::cast); }
constexpr bool is_conditional_set(Instruction insn) { return one_of(insn, Instruction::slt, Instruction::sltu, Instruction::sle, Instruction::seq); }

constexpr bool can_handle_types(std::initializer_list<Type> types) {
    for (auto type : types) {
        if (!is_integer(type) && type != Type::imm)
            return false;
    }
    return true;
}

Type resolve_simple_op_type(const Operation &op, [[maybe_unused]] size_t block_id, [[maybe_unused]] size_t var_id) {
#ifndef NDEBUG
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
        std::cerr << "Warning: Could not resolve definitive non-imm type (b" << block_id << "/v" << var_id << ")\n";
        return Type::i64;
    } else if (type_set.size() > 1) {
        std::cerr << "Warning: Type conflict detected (in b" << block_id << "/v" << var_id << ")! Types are:\n";
        Type largest = Type::i8;
        for (Type t : type_set) {
            std::cerr << " - " << t << '\n';
            int cmp = cast_dir(largest, t);
            assert(cmp != -1);
            if (cmp == 1) {
                largest = t;
            }
        }
        return largest;
    } else {
        return *type_set.begin();
    }
#else
    return op.out_vars[0]->type;
#endif
}

class ConstFoldPass {
    VarRewriter rewrite;
    BasicBlock *current_block;
    size_t var_index;

  public:
    void process_block(BasicBlock *block);

  private:
    SSAVar *new_imm(Type type, uint64_t imm, bool bin_rel = false);

    void replace_with_immediate(SSAVar *var, uint64_t immediate, bool bin_rel = false);
    void replace_var(SSAVar *var, SSAVar *new_var);

    struct BinOp {
        Instruction type;
        SSAVar *a, *b;
    };

    SSAVar *eval_to_imm(Instruction, Type, uint64_t, uint64_t, bool bin_rel = false);

    /** Simplify commutative operation. Used by simplify_bi_imm_{left,right} */
    void simplify_bi_comm(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in);
    /** Simplify a binary operation with an immediate on the left (e.g. v2 <- add 0, v1) */
    void simplify_bi_imm_left(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in);
    /** Simplify a binary operation with an immediate on the right (e.g. v2 <- add v1, 0) */
    void simplify_bi_imm_right(Instruction insn, Type type, uint64_t immediate, SSAVar *cur, SSAVar *in);

    /** Simplify a morphing operation (e.g. i32 v2 <- cast i32 v1) */
    void simplify_morph(Instruction insn, Type in_type, SSAVar *in_var, Type out_type, SSAVar *out_var);

    /** Simplify commutative operation with imm/op as parameters. Used by simplify_double_op_imm_{left,right} */
    std::optional<BinOp> simplify_double_op_comm(Type type, Instruction cins, const SSAVar::ImmInfo &imm, Instruction pins, SSAVar *pa, SSAVar *pb);
    /** Simplify an operation with an immediate on the left and an operation on the right */
    std::optional<BinOp> simplify_double_op_imm_left(Type type, Instruction cins, const SSAVar::ImmInfo &imm, Instruction pins, SSAVar *pa, SSAVar *pb);
    /** Simplify an operation with an immediate on the right and an operation on the left */
    std::optional<BinOp> simplify_double_op_imm_right(Type type, Instruction cins, const SSAVar::ImmInfo &imm, Instruction pins, SSAVar *pa, SSAVar *pb);

    void fixup_block();
};

SSAVar *ConstFoldPass::new_imm(Type type, uint64_t imm, bool bin_rel) {
    size_t new_id = current_block->cur_ssa_id++;
    auto new_var = std::make_unique<SSAVar>(new_id, imm, bin_rel);
    new_var->type = type;

    auto insert_point = std::next(current_block->variables.begin(), var_index);
    ++var_index;
    return current_block->variables.insert(insert_point, std::move(new_var))->get();
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

SSAVar *ConstFoldPass::eval_to_imm(Instruction insn, Type type, uint64_t a, uint64_t b, bool bin_rel) {
    auto result = eval_binary_op(insn, type, a, b);
    return new_imm(type, result, bin_rel);
}

// Note: Prefix notation in comments means IR operation, infix notation means operation on / evaluation of immediates.

std::optional<ConstFoldPass::BinOp> ConstFoldPass::simplify_double_op_comm(Type type, Instruction cins, const SSAVar::ImmInfo &imm, Instruction pins, SSAVar *pa, SSAVar *pb) {
    if (imm.binary_relative) {
        if ((pa->is_immediate() && pa->get_immediate().binary_relative) || (pb->is_immediate() && pb->get_immediate().binary_relative))
            return std::nullopt;

        if (cins == Instruction::add) {
            if (pins == Instruction::add) {
                if (pa->is_immediate()) {
                    // add (add pa:imm, pb:!imm), cb:imm[br]
                    auto *result = eval_to_imm(Instruction::add, type, pa->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::add, result, pb};
                } else if (pb->is_immediate()) {
                    // add (add pa:!imm, pb:imm), cb:imm[br]
                    auto *result = eval_to_imm(Instruction::add, type, pb->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::add, result, pa};
                }
            } else if (pins == Instruction::sub) {
                if (pa->is_immediate()) {
                    // add (sub pa:imm, pb:!imm), cb:imm[br]
                    auto *result = eval_to_imm(Instruction::add, type, pa->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::sub, result, pb};
                } else if (pb->is_immediate()) {
                    // add (sub pa:!imm, pb:imm), cb:imm[br]
                    auto *result = eval_to_imm(Instruction::sub, type, imm.val, pb->get_immediate().val, true);
                    return BinOp{Instruction::add, result, pa};
                }
            }
        }

        return std::nullopt;
    }

    if ((pa->is_immediate() && pa->get_immediate().binary_relative) || (pb->is_immediate() && pb->get_immediate().binary_relative)) {
        if (cins == Instruction::add) {
            if (pins == Instruction::add) {
                if (pa->is_immediate()) {
                    // add (add pa:imm[br], pb:!imm), cb
                    auto *result = eval_to_imm(Instruction::add, type, pa->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::add, result, pb};
                } else if (pb->is_immediate()) {
                    // add (add pa:!imm, pb:imm[br]), cb
                    auto *result = eval_to_imm(Instruction::add, type, pb->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::add, result, pa};
                }
            } else if (pins == Instruction::sub) {
                if (pa->is_immediate()) {
                    // add (sub pa:imm[br], pb:!imm), cb
                    auto *result = eval_to_imm(Instruction::add, type, pa->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::sub, result, pb};
                } else if (pb->is_immediate()) {
                    // add (sub pa:!imm, pb:imm[br]), cb
                    auto *result = eval_to_imm(Instruction::sub, type, pb->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::sub, pa, result};
                }
            }
        }

        return std::nullopt;
    }

    if (cins == Instruction::add) {
        if (pins == Instruction::add) {
            // add ca, (add pa, pb)
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // => add (pa + ca), pb
                auto *result = eval_to_imm(Instruction::add, type, pa->get_immediate().val, imm.val);
                return BinOp{Instruction::add, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // => add (pb + ca), pa
                auto *result = eval_to_imm(Instruction::add, type, pb->get_immediate().val, imm.val);
                return BinOp{Instruction::add, result, pa};
            }
        } else if (pins == Instruction::sub) {
            // add ca, (sub pa, pb)
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // => sub (pa + ca), pb
                auto *result = eval_to_imm(Instruction::add, type, pa->get_immediate().val, imm.val);
                return BinOp{Instruction::sub, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // => add (ca - pb), pa
                auto *result = eval_to_imm(Instruction::sub, type, imm.val, pb->get_immediate().val);
                return BinOp{Instruction::add, result, pa};
            }
        }
    } else if (cins == Instruction::_and) {
        if (pins == Instruction::_and) {
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // and ca, (and pa, pb)
                // => and (pa & ca), pb
                auto *result = eval_to_imm(Instruction::_and, type, pa->get_immediate().val, imm.val);
                return BinOp{Instruction::_and, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // and ca, (and pa, pb)
                // => and (pb & ca), pa
                auto *result = eval_to_imm(Instruction::_and, type, pb->get_immediate().val, imm.val);
                return BinOp{Instruction::_and, result, pa};
            }
        }
    } else if (cins == Instruction::_or) {
        if (pins == Instruction::_or) {
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // or ca, (or pa, pb)
                // => or (pa | ca), pb
                auto *result = eval_to_imm(Instruction::_or, type, pa->get_immediate().val, imm.val);
                return BinOp{Instruction::_or, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // or ca, (or pa, pb)
                // => or (pb | ca), pa
                auto *result = eval_to_imm(Instruction::_or, type, pb->get_immediate().val, imm.val);
                return BinOp{Instruction::_or, result, pa};
            }
        }
    } else if (cins == Instruction::_xor) {
        if (pins == Instruction::_xor) {
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // xor ca, (xor pa, pb)
                // => xor (pa ^ ca), pb
                auto *result = eval_to_imm(Instruction::_xor, type, pa->get_immediate().val, imm.val);
                return BinOp{Instruction::_xor, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // xor ca, (xor pa, pb)
                // => xor (pb ^ ca), pa
                auto *result = eval_to_imm(Instruction::_xor, type, pb->get_immediate().val, imm.val);
                return BinOp{Instruction::_xor, result, pa};
            }
        }
    }
    return std::nullopt;
}

std::optional<ConstFoldPass::BinOp> ConstFoldPass::simplify_double_op_imm_left(Type type, Instruction cins, const SSAVar::ImmInfo &imm, Instruction pins, SSAVar *pa, SSAVar *pb) {
    if (!is_binary_op(pins))
        return std::nullopt;
    if (!pa->is_immediate() && !pb->is_immediate())
        return std::nullopt;
    if (!can_handle_types({pa->type, pb->type}))
        return std::nullopt;
    assert(!(pa->is_immediate() && pb->is_immediate())); // Ops with imm/imm should be folded by now

    if (is_commutative(cins)) {
        return simplify_double_op_comm(type, cins, imm, pins, pa, pb);
    }

    if (imm.binary_relative) {
        if ((pa->is_immediate() && pa->get_immediate().binary_relative) || (pb->is_immediate() && pb->get_immediate().binary_relative))
            return std::nullopt;

        if (cins == Instruction::sub) {
            if (pins == Instruction::add) {
                if (pa->is_immediate()) {
                    // sub ca:imm[br], (add pa:imm, pb:!imm)
                    auto *result = eval_to_imm(Instruction::sub, type, imm.val, pa->get_immediate().val, true);
                    return BinOp{Instruction::sub, result, pb};
                } else if (pb->is_immediate()) {
                    // sub ca:imm[br], (add pa:!imm, pb:imm)
                    auto *result = eval_to_imm(Instruction::sub, type, imm.val, pb->get_immediate().val, true);
                    return BinOp{Instruction::sub, result, pa};
                }
            } else if (pins == Instruction::sub) {
                if (pa->is_immediate()) {
                    // sub ca:imm[br], (sub pa:imm, pb:!imm)
                    auto *result = eval_to_imm(Instruction::sub, type, imm.val, pa->get_immediate().val, true);
                    return BinOp{Instruction::add, result, pb};
                } else if (pb->is_immediate()) {
                    // sub ca:imm[br], (sub pa:!imm, pb:imm)
                    auto *result = eval_to_imm(Instruction::add, type, pb->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::sub, result, pa};
                }
            }
        }
        return std::nullopt;
    }
    if ((pa->is_immediate() && pa->get_immediate().binary_relative) || (pb->is_immediate() && pb->get_immediate().binary_relative)) {
        if (cins == Instruction::sub) {
            if (pins == Instruction::add) {
                // sub ca:imm, (add pa, pb)  (not simplifyable)
                return std::nullopt;
            } else if (pins == Instruction::sub) {
                if (pa->is_immediate()) {
                    // sub ca:imm, (sub pa:imm[br], pb:!imm)  (not simplifyable)
                    return std::nullopt;
                } else if (pb->is_immediate()) {
                    // sub ca:imm, (sub pa:!imm, pb:imm[br])
                    auto *result = eval_to_imm(Instruction::add, type, pb->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::sub, pa, result};
                }
            }
        }
        return std::nullopt;
    }

    if (cins == Instruction::sub) {
        if (pins == Instruction::add) {
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // sub ca, (add pa, pb)
                // => sub (ca - pa), pb
                auto *result = eval_to_imm(Instruction::sub, type, imm.val, pa->get_immediate().val);
                return BinOp{Instruction::sub, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // sub ca, (add pa, pb)
                // => sub (ca - pb), pa
                auto *result = eval_to_imm(Instruction::sub, type, imm.val, pb->get_immediate().val);
                return BinOp{Instruction::sub, result, pa};
            }
        } else if (pins == Instruction::sub) {
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // sub ca, (sub pa, pb)
                // => add (ca - pa), pb
                auto *result = eval_to_imm(Instruction::sub, type, imm.val, pa->get_immediate().val);
                return BinOp{Instruction::add, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // sub ca, (sub pa, pb)
                // => sub (pb + ca), pa
                auto *result = eval_to_imm(Instruction::add, type, pb->get_immediate().val, imm.val);
                return BinOp{Instruction::sub, result, pa};
            }
        }
    }
    return std::nullopt;
}

std::optional<ConstFoldPass::BinOp> ConstFoldPass::simplify_double_op_imm_right(Type type, Instruction cins, const SSAVar::ImmInfo &imm, Instruction pins, SSAVar *pa, SSAVar *pb) {
    if (!is_binary_op(pins))
        return std::nullopt;
    if (!pa->is_immediate() && !pb->is_immediate())
        return std::nullopt;
    if (!can_handle_types({pa->type, pb->type}))
        return std::nullopt;
    assert(!(pa->is_immediate() && pb->is_immediate()));

    if (is_commutative(cins)) {
        return simplify_double_op_comm(type, cins, imm, pins, pa, pb);
    }

    if (imm.binary_relative) {
        if ((pa->is_immediate() && pa->get_immediate().binary_relative) || (pb->is_immediate() && pb->get_immediate().binary_relative))
            return std::nullopt;

        if (cins == Instruction::sub) {
            if (pins == Instruction::add) {
                // sub (add pa, pb), cb:imm[br]  (not simplifyable)
                return std::nullopt;
            } else if (pins == Instruction::sub) {
                if (pa->is_immediate()) {
                    // sub (sub pa:imm, pb:!imm), cb:imm[br]  (not simplifyable)
                    return std::nullopt;
                } else if (pb->is_immediate()) {
                    // sub (sub pa:!imm, pb:imm), cb:imm[br]
                    auto *result = eval_to_imm(Instruction::add, type, imm.val, pb->get_immediate().val, true);
                    return BinOp{Instruction::sub, pa, result};
                }
            }
        }
        return std::nullopt;
    }
    if ((pa->is_immediate() && pa->get_immediate().binary_relative) || (pb->is_immediate() && pb->get_immediate().binary_relative)) {
        if (cins == Instruction::sub) {
            if (pins == Instruction::add) {
                if (pa->is_immediate()) {
                    // sub (add pa:imm[br], pb:!imm), cb
                    auto *result = eval_to_imm(Instruction::sub, type, pa->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::add, pb, result};
                } else if (pb->is_immediate()) {
                    // sub (add pa:!imm, pb:imm[br]), cb
                    auto *result = eval_to_imm(Instruction::sub, type, pb->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::add, pa, result};
                }
            } else if (pins == Instruction::sub) {
                if (pa->is_immediate()) {
                    // sub (sub pa:imm[br], pb:!imm), cb
                    auto *result = eval_to_imm(Instruction::sub, type, pa->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::sub, result, pb};
                } else if (pb->is_immediate()) {
                    // sub (sub pa:!imm, pb:imm[br]), cb
                    auto *result = eval_to_imm(Instruction::add, type, pb->get_immediate().val, imm.val, true);
                    return BinOp{Instruction::sub, pa, result};
                }
            }
        }
        return std::nullopt;
    }

    if (cins == Instruction::sub) {
        if (pins == Instruction::add) {
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // sub (add pa, pb), cb
                // => add (pa - cb), pb
                auto *result = eval_to_imm(Instruction::sub, type, pa->get_immediate().val, imm.val);
                return BinOp{Instruction::add, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // sub (add pa, pb), cb
                // => add (pb - cb), pa
                auto *result = eval_to_imm(Instruction::sub, type, pb->get_immediate().val, imm.val);
                return BinOp{Instruction::add, result, pa};
            }
        } else if (pins == Instruction::sub) {
            if (pa->is_immediate()) {
                // pa: imm, pb: not imm
                // sub (sub pa, pb), cb
                // => sub (pa - cb), pb
                auto *result = eval_to_imm(Instruction::sub, type, pa->get_immediate().val, imm.val);
                return BinOp{Instruction::sub, result, pb};
            } else if (pb->is_immediate()) {
                // pa: not imm, pb: imm
                // sub (sub pa, pb), cb
                // => sub pa, (pb + cb)
                auto *result = eval_to_imm(Instruction::add, type, pb->get_immediate().val, imm.val);
                return BinOp{Instruction::sub, pa, result};
            }
        }
    }
    return std::nullopt;
}

void ConstFoldPass::replace_with_immediate(SSAVar *var, uint64_t imm, bool bin_rel) { var->info = SSAVar::ImmInfo{static_cast<int64_t>(imm), bin_rel}; }

void ConstFoldPass::replace_var(SSAVar *var, SSAVar *new_var) { rewrite.replace(var, new_var); }

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
        } else if (typed_equal(type, immediate, UINT64_MAX)) {
            auto &op = cur->get_operation();
            op.type = Instruction::_not;
            op.set_inputs(in);
        }
        break;
    default:
        break;
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

void ConstFoldPass::process_block(BasicBlock *block) {
    rewrite = {};
    current_block = block;

    for (var_index = 0; var_index < block->variables.size(); var_index++) {
        auto *var = block->variables[var_index].get();
        if (!var->is_operation())
            continue;

        auto &op = var->get_operation();
        rewrite.apply_to(op);

        if (is_binary_op(op.type)) {
            auto &a = op.in_vars[0], &b = op.in_vars[1];
            if (!can_handle_types({a->type, b->type}))
                continue;
            Type type = resolve_simple_op_type(op, block->id, var->id);
            if (a->is_immediate() && b->is_immediate()) {
                // op imm, imm
                auto &ia = a->get_immediate(), &ib = b->get_immediate();

                // Evaluating binary-relative immediates only makes sense in three cases:
                // `a + (bin_rel b)`, `(bin_rel a) + b` and `(bin_rel a) - b`.
                if (ia.binary_relative && ib.binary_relative)
                    continue;
                bool bin_rel = ia.binary_relative || ib.binary_relative;
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
                replace_with_immediate(var, result, bin_rel);
            } else {
                if (a->is_immediate() && b->is_operation()) {
                    const auto &ai = a->get_immediate();
                    const auto &bo = b->get_operation();
                    // op imm, op
                    if (auto nw = simplify_double_op_imm_left(type, op.type, ai, bo.type, bo.in_vars[0].get(), bo.in_vars[1].get())) {
                        op.type = nw->type;
                        op.set_inputs(nw->a, nw->b);
                    }
                } else if (b->is_immediate() && a->is_operation()) {
                    const auto &ao = a->get_operation();
                    const auto &bi = b->get_immediate();
                    // op op, imm
                    if (auto nw = simplify_double_op_imm_right(type, op.type, bi, ao.type, ao.in_vars[0].get(), ao.in_vars[1].get())) {
                        op.type = nw->type;
                        op.set_inputs(nw->a, nw->b);
                    }
                }

                // simplify_double_op_* can change the order of parameters; thus the separate check
                if (a->is_immediate()) {
                    const auto &imm = a->get_immediate();
                    if (imm.binary_relative)
                        continue;
                    // op imm, any
                    simplify_bi_imm_left(op.type, type, imm.val, var, b);
                } else if (b->is_immediate()) {
                    const auto &imm = b->get_immediate();
                    if (imm.binary_relative)
                        continue;
                    // op any, imm
                    simplify_bi_imm_right(op.type, type, imm.val, var, a);
                }

                if ((op.type == Instruction::_xor || op.type == Instruction::sub) && a.get() == b.get()) {
                    // xor a, a = 0, sub a, a = 0
                    replace_with_immediate(var, 0);
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
                Type type = resolve_simple_op_type(op, block->id, var->id);
                int64_t result = eval_unary_op(op.type, type, ii.val);
                replace_with_immediate(var, result);
            } else if (in->is_operation()) {
                auto &prev = in->get_operation();
                if (prev.type == Instruction::_not) {
                    // not (not x) = x
                    replace_var(var, prev.in_vars[0]);
                }
            }
        } else if (is_morphing_op(op.type)) {
            auto &in = op.in_vars[0];
            assert(op.out_vars[0] == var);
            Type input = in->type, output = var->type;
            if (!can_handle_types({input, output}))
                continue;
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
                replace_with_immediate(var, result);
            } else {
                if (in->is_operation()) {
                    auto &po = in->get_operation();
                    if (op.type == Instruction::cast && (po.type == Instruction::sign_extend || po.type == Instruction::zero_extend)) {
                        auto *pin = po.in_vars[0].get();
                        // in <- extend pin
                        // var <- cast in
                        if (pin->type == var->type) {
                            rewrite.replace(var, pin);
                            continue;
                        } else if (cast_dir(pin->type, var->type) == 0) {
                            op.in_vars[0] = pin;
                        }
                    }
                }
                // op any
                simplify_morph(op.type, input, in, output, var);
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
                Type type = resolve_simple_op_type(op, block->id, var->id);
                auto [div_result, rem_result] = eval_div(op.type, type, b->get_immediate().val, a->get_immediate().val);
                int64_t result = op.out_vars[0] ? div_result : rem_result;
                replace_with_immediate(var, result);
            }
        } else if (is_conditional_set(op.type)) {
            auto &a = op.in_vars[0], &b = op.in_vars[1], &val_if_true = op.in_vars[2], &val_if_false = op.in_vars[3];
            if (!can_handle_types({a->type, b->type}))
                continue;
            if (a->is_immediate() && b->is_immediate()) {
                // setcond imm, imm, any, any
                bool is_signed = op.type == Instruction::slt || op.type == Instruction::sle;
                Type type;
                if (a->type != b->type) {
                    std::cerr << "Warning: Type mismatch on conditional set operation, using largest\n";
                    int cmp = cast_dir(a->type, b->type);
                    assert(cmp != -1);
                    type = cmp == 1 ? b->type : a->type;
                } else if (a->type == Type::imm && is_signed) {
                    std::cerr << "Warning: signed conditional set operation with imm values, treating as i64\n";
                    type = Type::i64;
                } else {
                    type = a->type;
                }
                int cmp = typed_compare(type, a->get_immediate().val, b->get_immediate().val, is_signed);
                bool is_true;
                switch (op.type) {
                case Instruction::slt:
                case Instruction::sltu:
                    is_true = cmp < 0;
                    break;
                case Instruction::seq:
                    is_true = cmp == 0;
                    break;
                case Instruction::sle:
                    is_true = cmp <= 0;
                    break;
                default:
                    unreachable();
                }
                replace_var(var, is_true ? val_if_true : val_if_false);
            }
        }
    }

    // Apply rewrites to control flow ops
    rewrite.apply_to(block->control_flow_ops);

    fixup_block();
}

void ConstFoldPass::fixup_block() {
#if 0
    // Currently, the generator fails if an immediate hasn't type Type::imm
    for (auto &var : current_block->variables) {
        if (var->is_immediate())
            var->type = Type::imm;
    }
#endif
}

} // namespace

void const_fold(IR *ir) {
    ConstFoldPass pass;
    for (auto &bb : ir->basic_blocks) {
        pass.process_block(bb.get());
    }
}

} // namespace optimizer
