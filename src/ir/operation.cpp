#include "ir/operation.h"

#include "ir/basic_block.h"
#include "ir/ir.h"
#include "ir/variable.h"

#include <cassert>

template <typename T> static size_t count_non_null(const T &container) {
    return std::count_if(std::begin(container), std::end(container), [](const auto &it) { return it; });
}

void Operation::set_inputs(SSAVar *in1, SSAVar *in2, SSAVar *in3, SSAVar *in4) {
    in_vars[0] = in1;
    in_vars[1] = in2;
    in_vars[2] = in3;
    in_vars[3] = in4;
}

void Operation::set_outputs(SSAVar *out1, SSAVar *out2, SSAVar *out3) {
    out_vars[0] = out1;
    out_vars[1] = out2;
    out_vars[2] = out3;
}

void Operation::set_inputs(std::initializer_list<SSAVar *> inputs) {
    assert(count_non_null(in_vars) == 0);
    assert(inputs.size() <= in_vars.size());

    std::copy(inputs.begin(), inputs.end(), in_vars.begin());
}

void Operation::set_outputs(std::initializer_list<SSAVar *> outputs) {
    assert(count_non_null(out_vars) == 0);
    assert(outputs.size() <= out_vars.size());

    std::copy(outputs.begin(), outputs.end(), out_vars.begin());
}

void Operation::print(std::ostream &stream, const IR *ir) const {
    stream << type << " ";

    bool first = true;
    for (auto &in : in_vars) {
        if (in) {
            if (!first)
                stream << ", ";
            else
                first = false;
            in->print_type_name(stream, ir);
        }
    }
    if (std::holds_alternative<SSAVar *>(rounding_info)) {
        stream << "(rm = ";
        std::get<SSAVar *>(rounding_info)->print_type_name(stream, ir);
        stream << ")";
    } else if (std::holds_alternative<RoundingMode>(rounding_info)) {
        stream << "(rm = " << static_cast<uint32_t>(std::get<RoundingMode>(rounding_info)) << ")";
    }
}

namespace {
std::unique_ptr<Operation> create_op(Instruction type, std::initializer_list<SSAVar *> inputs, std::initializer_list<SSAVar *> outputs) {
    auto op = std::make_unique<Operation>(type);
    op->set_inputs(inputs);
    op->set_outputs(outputs);
    return op;
}

constexpr bool is_address_type(Type type) { return type == Type::i32 || type == Type::i64; }
} // namespace

std::unique_ptr<Operation> Operation::new_store(SSAVar *out_memory_token, SSAVar *in_address, SSAVar *in_value, SSAVar *in_memory_token) {
    assert(out_memory_token && in_address && in_value && in_memory_token);

    assert(is_address_type(in_address->type));
    assert(in_memory_token->type == Type::mt);
    assert(out_memory_token->type == Type::mt);

    return create_op(Instruction::store, {in_address, in_value, in_memory_token}, {out_memory_token});
}
std::unique_ptr<Operation> Operation::new_load(SSAVar *out_result, SSAVar *in_address, SSAVar *in_memory_token) {
    assert(out_result && in_address && in_memory_token);

    assert(is_address_type(in_address->type));
    assert(in_memory_token->type == Type::mt);

    return create_op(Instruction::load, {in_address, in_memory_token}, {out_result});
}
std::unique_ptr<Operation> Operation::new_add(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);

    return create_op(Instruction::add, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_sub(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);

    return create_op(Instruction::sub, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_mul_l(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);
    return create_op(Instruction::mul_l, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_ssmul_h(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);
    return create_op(Instruction::ssmul_h, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_uumul_h(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);
    return create_op(Instruction::uumul_h, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_sumul_h(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);
    return create_op(Instruction::sumul_h, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_div(SSAVar *out_result, SSAVar *out_remainder, SSAVar *in_a, SSAVar *in_b) {
    // One of result or remainder can be set to null
    assert((out_result || out_remainder) && in_a && in_b);
    return create_op(Instruction::div, {in_a, in_b}, {out_result, out_remainder});
}
std::unique_ptr<Operation> Operation::new_udiv(SSAVar *out_result, SSAVar *out_remainder, SSAVar *in_a, SSAVar *in_b) {
    assert((out_result || out_remainder) && in_a && in_b);
    return create_op(Instruction::udiv, {in_a, in_b}, {out_result, out_remainder});
}
std::unique_ptr<Operation> Operation::new_shl(SSAVar *out_result, SSAVar *in_value, SSAVar *in_amount) {
    assert(out_result && in_value && in_amount);
    return create_op(Instruction::shl, {in_value, in_amount}, {out_result});
}
std::unique_ptr<Operation> Operation::new_shr(SSAVar *out_result, SSAVar *in_value, SSAVar *in_amount) {
    assert(out_result && in_value && in_amount);
    return create_op(Instruction::shr, {in_value, in_amount}, {out_result});
}
std::unique_ptr<Operation> Operation::new_sar(SSAVar *out_result, SSAVar *in_value, SSAVar *in_amount) {
    assert(out_result && in_value && in_amount);
    return create_op(Instruction::sar, {in_value, in_amount}, {out_result});
}
std::unique_ptr<Operation> Operation::new_or(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);
    return create_op(Instruction::_or, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_and(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);
    return create_op(Instruction::_and, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_not(SSAVar *out_result, SSAVar *in_value) {
    assert(out_result && in_value);
    return create_op(Instruction::_not, {in_value}, {out_result});
}
std::unique_ptr<Operation> Operation::new_xor(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b) {
    assert(out_result && in_a && in_b);
    return create_op(Instruction::_xor, {in_a, in_b}, {out_result});
}
std::unique_ptr<Operation> Operation::new_cast(SSAVar *out_result, SSAVar *in_value) {
    assert(out_result && in_value);
    return create_op(Instruction::cast, {in_value}, {out_result});
}
std::unique_ptr<Operation> Operation::new_sltu(SSAVar *out_result, SSAVar *in_a, SSAVar *in_b, SSAVar *in_value_if_less, SSAVar *in_value_otherwise) {
    assert(out_result && in_a && in_b && in_value_if_less && in_value_otherwise);
    return create_op(Instruction::sltu, {in_a, in_b, in_value_if_less, in_value_otherwise}, {out_result});
}
std::unique_ptr<Operation> Operation::new_sign_extend(SSAVar *out_result, SSAVar *in_value) {
    assert(out_result && in_value);
    return create_op(Instruction::sign_extend, {in_value}, {out_result});
}
std::unique_ptr<Operation> Operation::new_zero_extend(SSAVar *out_result, SSAVar *in_value) {
    assert(out_result && in_value);
    return create_op(Instruction::zero_extend, {in_value}, {out_result});
}
std::unique_ptr<Operation> Operation::new_setup_stack(SSAVar *out_sp) {
    assert(out_sp);
    return create_op(Instruction::setup_stack, {}, {out_sp});
}

CfOp::CfOp(const CFCInstruction type, BasicBlock *source, BasicBlock *target) : type(type), source(source), in_vars() {
    if (target != nullptr) {
        target->predecessors.push_back(source);
        source->successors.push_back(target);
    }

    switch (type) {
    case CFCInstruction::jump:
        info = JumpInfo{};
        std::get<JumpInfo>(info).target = target;
        break;
    case CFCInstruction::ijump:
        info = IJumpInfo{};
        if (target != nullptr) {
            assert(0); // <- only enable this if you are sure about what you are doing.
            // this is dangerous because one has to remember to also emplace the corresponding target mapping in mappings!
            std::get<IJumpInfo>(info).targets.emplace_back(target);
        }
        break;
    case CFCInstruction::cjump:
        info = CJumpInfo{};
        std::get<CJumpInfo>(info).target = target;
        break;
    case CFCInstruction::call:
        info = CallInfo{};
        std::get<CallInfo>(info).target = target;
        break;
    case CFCInstruction::icall:
        info = ICallInfo{};
        break;
    case CFCInstruction::_return:
        info = RetInfo{};
        break;
    case CFCInstruction::unreachable:
        break;
    case CFCInstruction::syscall:
        info = SyscallInfo{};
        std::get<SyscallInfo>(info).continuation_block = target;
        break;
    }
}

void CfOp::set_inputs(SSAVar *op1, SSAVar *op2, SSAVar *op3, SSAVar *op4, SSAVar *op5, SSAVar *op6, SSAVar *op7) {
    assert(in_vars[0] == nullptr && in_vars[1] == nullptr && in_vars[2] == nullptr && in_vars[3] == nullptr && in_vars[4] == nullptr && in_vars[5] == nullptr && in_vars[6] == nullptr);
    in_vars[0] = op1;
    in_vars[1] = op2;
    in_vars[2] = op3;
    in_vars[3] = op4;
    in_vars[4] = op5;
    in_vars[5] = op6;
    in_vars[6] = op7;
}

void CfOp::add_target_input(SSAVar *input, size_t static_idx) {
    switch (type) {
    case CFCInstruction::jump:
        std::get<JumpInfo>(info).target_inputs.emplace_back(input);
        break;
    case CFCInstruction::cjump:
        std::get<CJumpInfo>(info).target_inputs.emplace_back(input);
        break;
    case CFCInstruction::call:
        std::get<CallInfo>(info).target_inputs.emplace_back(input);
        break;
    case CFCInstruction::icall:
        std::get<ICallInfo>(info).mapping.emplace_back(input, static_idx);
        break;
    case CFCInstruction::syscall:
        std::get<SyscallInfo>(info).continuation_mapping.emplace_back(input, static_idx);
        break;
    case CFCInstruction::_return:
        std::get<RetInfo>(info).mapping.emplace_back(input, static_idx);
        break;
    case CFCInstruction::ijump:
        std::get<IJumpInfo>(info).mapping.emplace_back(input, static_idx);
        break;
    case CFCInstruction::unreachable:
        assert(0);
        break;
    }
}

void CfOp::clear_target_inputs() {
    switch (type) {
    case CFCInstruction::jump:
        std::get<CfOp::JumpInfo>(info).target_inputs.clear();
        break;
    case CFCInstruction::cjump:
        std::get<CfOp::CJumpInfo>(info).target_inputs.clear();
        break;
    case CFCInstruction::call:
        std::get<CfOp::CallInfo>(info).target_inputs.clear();
        break;
    case CFCInstruction::syscall:
        std::get<CfOp::SyscallInfo>(info).continuation_mapping.clear();
        break;
    case CFCInstruction::_return:
        std::get<CfOp::RetInfo>(info).mapping.clear();
        break;
    case CFCInstruction::icall:
        std::get<CfOp::ICallInfo>(info).mapping.clear();
        break;
    case CFCInstruction::ijump:
        std::get<CfOp::IJumpInfo>(info).mapping.clear();
        break;
    default:
        assert(0);
    }
}

void CfOp::set_target(BasicBlock *target) {
    assert(type == CFCInstruction::jump || type == CFCInstruction::cjump || type == CFCInstruction::ijump || type == CFCInstruction::call || type == CFCInstruction::syscall ||
           type == CFCInstruction::icall);

    switch (type) {
    case CFCInstruction::jump:
        std::get<JumpInfo>(info).target = target;
        break;
    case CFCInstruction::cjump:
        std::get<CJumpInfo>(info).target = target;
        break;
    case CFCInstruction::call:
        std::get<CallInfo>(info).target = target;
        break;
    case CFCInstruction::syscall:
        std::get<SyscallInfo>(info).continuation_block = target;
        break;
    case CFCInstruction::unreachable:
    case CFCInstruction::_return:
    case CFCInstruction::ijump:
    case CFCInstruction::icall:
        assert(0);
        break;
    }
}

BasicBlock *CfOp::target() const {
    switch (type) {
    case CFCInstruction::jump:
        return std::get<JumpInfo>(info).target;
    case CFCInstruction::cjump:
        return std::get<CJumpInfo>(info).target;
    case CFCInstruction::call:
        return std::get<CallInfo>(info).target;
    case CFCInstruction::syscall:
        return std::get<SyscallInfo>(info).continuation_block;
    case CFCInstruction::icall:
    case CFCInstruction::ijump:
    case CFCInstruction::unreachable:
    case CFCInstruction::_return:
        return nullptr;
    }

    assert(0);
    return nullptr;
}

BasicBlock *CfOp::continuation_target() const {
    switch (type) {
    case CFCInstruction::jump:
        return std::get<JumpInfo>(info).target;
    case CFCInstruction::cjump:
        return std::get<CJumpInfo>(info).target;
    case CFCInstruction::call:
        return std::get<CallInfo>(info).continuation_block;
    case CFCInstruction::syscall:
        return std::get<SyscallInfo>(info).continuation_block;
    case CFCInstruction::ijump:
        return std::get<IJumpInfo>(info).target;
    case CFCInstruction::icall:
        return std::get<ICallInfo>(info).continuation_block;
    case CFCInstruction::unreachable:
    case CFCInstruction::_return:
        return nullptr;
    }

    assert(0);
    return nullptr;
}

const std::vector<SSAVar *> &CfOp::target_inputs() const {
    static std::vector<SSAVar *> vec{};
    vec.clear();

    std::visit(
        [](auto &i) {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, JumpInfo> || std::is_same_v<T, CJumpInfo> || std::is_same_v<T, CallInfo>) {
                vec.reserve(i.target_inputs.size());
                for (auto &var : i.target_inputs) {
                    vec.push_back(var.get());
                }
            } else if constexpr (std::is_same_v<T, SyscallInfo>) {
                vec.reserve(i.continuation_mapping.size());
                for (auto &var : i.continuation_mapping) {
                    vec.push_back(var.first.get());
                }
            } else if constexpr (std::is_same_v<T, IJumpInfo> || std::is_same_v<T, ICallInfo>) {
                vec.reserve(i.mapping.size());
                for (auto &var : i.mapping) {
                    vec.push_back(var.first.get());
                }
            }
        },
        info);
    return vec;
}

size_t CfOp::target_input_count() const {
    return std::visit(
        [](auto &i) -> size_t {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, JumpInfo> || std::is_same_v<T, CJumpInfo> || std::is_same_v<T, CallInfo>) {
                return i.target_inputs.size();
            } else if constexpr (std::is_same_v<T, SyscallInfo>) {
                return i.continuation_mapping.size();
            } else if constexpr (std::is_same_v<T, IJumpInfo> || std::is_same_v<T, ICallInfo>) {
                return i.mapping.size();
            } else {
                return 0;
            }
        },
        info);
}

void CfOp::print(std::ostream &stream, const IR *ir) const {
    stream << '(' << type << ", [";
    if (type == CFCInstruction::_return) {
        auto first = true;
        const auto &ret_info = std::get<RetInfo>(info);
        for (const auto &[var, s_idx] : ret_info.mapping) {
            if (!first) {
                stream << ", ";
            } else {
                first = false;
            }

            stream << ir->statics[s_idx].type << " @" << ir->statics[s_idx].id << " <- ";
            var->print_type_name(stream, ir);
        }

        stream << "])";
        return;
    }

    if (type != CFCInstruction::ijump) {
        const auto *target = this->target();
        if (target) {
            target->print_name(stream, ir);

            if (type != CFCInstruction::syscall) {
                for (const auto &var : target_inputs()) {
                    stream << ", ";
                    var->print_type_name(stream, ir);
                }
            }
        }
    }
    stream << "]";

    for (const auto &var : in_vars) {
        if (!var)
            continue;
        stream << ", ";
        var->print_type_name(stream, ir);
    }

    if (info.index() == 1) {
        // TODO
        stream << ", ";
        switch (info.index()) {
        case 1:
            std::get<CfOp::CJumpInfo>(info).print(stream);
            break;
        }
    }

    stream << ')';
}

void CfOp::CJumpInfo::print(std::ostream &stream) const {
    switch (type) {
    case CJumpType::eq:
        stream << "eq";
        break;
    case CJumpType::neq:
        stream << "neq";
        break;
    case CJumpType::lt:
        stream << "lt";
        break;
    case CJumpType::gt:
        stream << "gt";
        break;
    case CJumpType::slt:
        stream << "slt";
        break;
    case CJumpType::sgt:
        stream << "sgt";
        break;
    }
}
