#include "generator/x86_64/generator.h"

using namespace generator::x86_64;

namespace {
enum REGISTER {
    REG_A,
    REG_B,
    REG_C,
    REG_D,
    REG_DI,
    REG_SI,
    REG_8,
    REG_9,
    REG_10,
    REG_11,
    REG_12,
    REG_13,
    REG_14,
    REG_15,

    REG_COUNT,
    REG_NONE
};

std::array<std::array<const char*, 4>, REG_COUNT> reg_names = {
    std::array<const char*, 4>{ "rax", "eax", "ax", "al" },
    { "rbx", "ebx", "bx", "bl" },
    { "rcx", "ecx", "cx", "cl" },
    { "rdx", "edx", "dx", "dl" },
    { "rdi", "edi", "di", "dil" },
    { "rsi", "esi", "si", "sil" },
    { "r8", "r8d", "r8w", "r8b" },
    { "r9", "r9d", "r9w", "r9b" },
    { "r10", "r10d", "r10w", "r10b" },
    { "r11", "r11d", "r11w", "r11b" },
    { "r12", "r12d", "r12w", "r12b" },
    { "r13", "r13d", "r13w", "r13b" },
    { "r14", "r14d", "r14w", "r14b" },
    { "r15", "r15d", "r15w", "r15b" },
};

std::array<REGISTER, 6> call_reg = {REG_DI, REG_SI, REG_D, REG_C, REG_8, REG_9};

// only for integers
const char* reg_name(const REGISTER reg, const Type type) {
    const auto& arr = reg_names[reg];
    switch (type) {
        case Type::i64:
            return arr[0];
        case Type::i32:
            return arr[1];
        case Type::i16:
            return arr[2];
        case Type::i8:
            return arr[3];
        default:
            assert(0);
            exit(1);
    }
}

const char* mem_size(const Type type) {
    switch (type) {
    case Type::imm:
    case Type::i64:
        return "QWORD PTR";
    case Type::i32:
        return "DWORD PTR";
    case Type::i16:
        return "WORD PTR";
    case Type::i8:
        return "BYTE PTR";
    case Type::f32:
    case Type::f64:
    case Type::mt:
        assert(0);
        exit(1);
    }

    assert(0);
    exit(1);
}

struct RegInfo {
    SSAVar* current_var = nullptr;
};

struct VarInfo {
    REGISTER cur_reg = REG_NONE;
    size_t last_used = 0;
    std::vector<size_t> uses = {};
};

size_t index_for_var(const BasicBlock *block, const SSAVar *var) {
    for (size_t idx = 0; idx < block->variables.size(); ++idx) {
        if (block->variables[idx].get() == var)
            return idx;
    }

    assert(0);
    exit(1);
}

uint64_t const_eval(const Instruction instr, const uint64_t val1, const uint64_t val2, const Type type) {
    switch (instr) {
        case Instruction::add:
            switch (type) {
                case Type::i64:
                    return val1 + val2;
                case Type::i32:
                    return static_cast<uint32_t>(val1) + static_cast<uint32_t>(val2);
                case Type::i16:
                    return static_cast<uint16_t>(val1) + static_cast<uint16_t>(val2);
                case Type::i8:
                    return static_cast<uint8_t>(val1) + static_cast<uint8_t>(val1);
                default:
                    assert(0);
                    exit(1);
            }
            break;
        case Instruction::sub:
            switch (type) {
                case Type::i64:
                    return val1 - val2;
                case Type::i32:
                    return static_cast<uint32_t>(val1) - static_cast<uint32_t>(val2);
                case Type::i16:
                    return static_cast<uint16_t>(val1) - static_cast<uint16_t>(val2);
                case Type::i8:
                    return static_cast<uint8_t>(val1) - static_cast<uint8_t>(val1);
                default:
                    assert(0);
                    exit(1);
            }
            break;
        case Instruction::mul_l:
            switch (type) {
                case Type::i64:
                    return val1 * val2;
                case Type::i32:
                    return static_cast<uint32_t>(val1) * static_cast<uint32_t>(val2);
                case Type::i16:
                    return static_cast<uint16_t>(val1) * static_cast<uint16_t>(val2);
                case Type::i8:
                    return static_cast<uint8_t>(val1) * static_cast<uint8_t>(val1);
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::ssmul_h:
            switch (type) {
                case Type::i64: {
                    const __int128_t a = static_cast<int64_t>(val1);
                    const __int128_t b = static_cast<int64_t>(val2);
                    const auto res_full = a * b;
                    return static_cast<uint64_t>(res_full >> 64);
                }
                case Type::i32: {
                    const auto a = static_cast<int64_t>(static_cast<int32_t>(val1));
                    const auto b = static_cast<int64_t>(static_cast<int32_t>(val2));
                    const auto res = a * b;
                    // TODO: does this need to be sign-extended?
                    return static_cast<uint32_t>(res >> 32);
                }
                case Type::i16: {
                    const auto a = static_cast<int32_t>(static_cast<int16_t>(val1));
                    const auto b = static_cast<int32_t>(static_cast<int16_t>(val2));
                    const auto res = a * b;
                    // TODO: does this need to be sign-extended?
                    return static_cast<uint16_t>(res >> 16);
                }
                case Type::i8: {
                    const auto a = static_cast<int16_t>(static_cast<int8_t>(val1));
                    const auto b = static_cast<int16_t>(static_cast<int8_t>(val2));
                    const auto res = a * b;
                    // TODO: does this need to be sign-extended?
                    return static_cast<uint8_t>(res >> 8);
                }
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::uumul_h:
            switch (type) {
                case Type::i64: {
                    const __uint128_t a = val1;
                    const __uint128_t b = val2;
                    const auto res = a * b;
                    return static_cast<uint64_t>(res >> 64);
                }
                case Type::i32: {
                    const auto a = val1 & 0xFFFFFFFF;
                    const auto b = val2 & 0xFFFFFFFF;
                    const auto res = a * b;
                    return static_cast<uint32_t>(res >> 32);
                }
                case Type::i16: {
                    const auto a = val1 & 0xFFFF;
                    const auto b = val2 & 0xFFFF;
                    const auto res = (a * b) & 0xFFFFFFFF;
                    return static_cast<uint16_t>(res >> 16);
                }
                case Type::i8: {
                    const auto a = val1 & 0xFF;
                    const auto b = val2 & 0xFF;
                    const auto res = (a * b) & 0xFFFF;
                    return static_cast<uint8_t>(res >> 8);
                }
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::sumul_h:
            switch (type) {
                case Type::i64: {
                    const __int128_t a = static_cast<int64_t>(val1);
                    const __uint128_t b = val2;
                    const auto res = a * b;
                    return static_cast<uint64_t>(res >> 64);
                }
                case Type::i32: {
                    const int64_t a = static_cast<int32_t>(val1);
                    const auto res = a * val2;
                    return static_cast<uint32_t>(res >> 32);
                }
                case Type::i16: {
                    const int32_t a = static_cast<int16_t>(val1);
                    const uint32_t b = val2;
                    const auto res = a * b;
                    return static_cast<uint16_t>(res >> 16);
                }
                case Type::i8: {
                    const int16_t a = static_cast<int8_t>(val1);
                    const uint16_t b = val2;
                    const auto res = a * b;
                    return static_cast<uint8_t>(res >> 8);
                }
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::div:
            switch (type) {
                case Type::i64:
                    return static_cast<int64_t>(val1) / static_cast<int64_t>(val2);
                case Type::i32:
                    return static_cast<int32_t>(val1) / static_cast<int32_t>(val2);
                case Type::i16:
                    return static_cast<int16_t>(val1) / static_cast<int16_t>(val2);
                case Type::i8:
                    return static_cast<int8_t>(val1) / static_cast<int8_t>(val2);
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::udiv:
            switch (type) {
                case Type::i64:
                    return val1 / val2;
                case Type::i32:
                    return (val1 & 0xFFFFFFFF) / (val2 & 0xFFFFFFFF);
                case Type::i16:
                    return (val1 & 0xFFFF) / (val2 & 0xFFFF);
                case Type::i8:
                    return (val1 & 0xFF) / (val2 & 0xFF);
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::shl:
            switch (type) {
                case Type::i64:
                    return val1 << val2;
                case Type::i32:
                    return static_cast<uint32_t>(val1) << static_cast<uint32_t>(val2);
                case Type::i16:
                    return static_cast<uint16_t>(val1) << static_cast<uint16_t>(val2);
                case Type::i8:
                    return static_cast<uint8_t>(val1) << static_cast<uint8_t>(val2);
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::shr:
            switch (type) {
                case Type::i64:
                    return val1 >> val2;
                case Type::i32:
                    return static_cast<uint32_t>(val1) >> static_cast<uint32_t>(val2);
                case Type::i16:
                    return static_cast<uint16_t>(val1) >> static_cast<uint16_t>(val2);
                case Type::i8:
                    return static_cast<uint8_t>(val1) >> static_cast<uint8_t>(val2);
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::sar:
            switch (type) {
                case Type::i64:
                    return static_cast<int64_t>(val1) >> val2;
                case Type::i32:
                    return static_cast<int32_t>(val1) >> static_cast<uint32_t>(val2);
                case Type::i16:
                    return static_cast<int16_t>(val1) >> static_cast<uint16_t>(val2);
                case Type::i8:
                    return static_cast<int8_t>(val1) >> static_cast<uint8_t>(val2);
                default:
                    assert(0);
                    exit(1);
            }
        case Instruction::_or:
            return val1 | val2;
        case Instruction::_and:
            return val1 & val2;
        case Instruction::_xor:
            return val1 ^ val2;
        case Instruction::max:
            return std::max(val1, val2);
        case Instruction::min:
            return std::min(val1, val2);
        case Instruction::smax:
            return std::max(static_cast<int64_t>(val1), static_cast<int64_t>(val2));
        case Instruction::smin:
            return std::min(static_cast<int64_t>(val1), static_cast<int64_t>(val2));
        default:
            assert(0);
            exit(1);
    }
}
}

// TODO: binary-relative immediates
void Generator::compile_block_reg_alloc(const BasicBlock* bb) {
    auto reg_map = std::array<RegInfo, REG_COUNT>{};
    auto var_map = std::vector<VarInfo>{}; // TODO: move this into SSA-Var? could also keep idx there for easy access
    var_map.resize(bb->variables.size());

    // allocates a register for any value
    const auto alloc_reg = [&reg_map, &var_map, bb, this] (size_t cur_time, REGISTER only_this_reg = REG_NONE, REGISTER clear_reg = REG_NONE) -> REGISTER {
        REGISTER reg = only_this_reg;

        if (reg == REG_NONE) {
            // try to find free register
            for (size_t i = 0; i < REG_COUNT; ++i) {
                if (static_cast<REGISTER>(i) != clear_reg && reg_map[i].current_var == nullptr) {
                    reg = static_cast<REGISTER>(i);
                    break;
                }
            }
        
            if (reg == REG_NONE) {
                // otherwise try to find unused var
                for (size_t i = 0; i < REG_COUNT; ++i) {
                    if (static_cast<REGISTER>(i) == clear_reg) {
                        continue;
                    }

                    const auto idx = index_for_var(bb, reg_map[i].current_var);
                    if (var_map[idx].uses.empty() || var_map[idx].last_used < cur_time) {
                        reg = static_cast<REGISTER>(i);
                        break;
                    }
                }

                if (reg == REG_NONE) {
                    // otherwise find var with the largest time of next use
                    SSAVar* last_used_var = nullptr;
                    size_t last_used_idx = 0;
                    size_t last_use_time = 0;

                    for (size_t i = 0; i < REG_COUNT; ++i) {
                        if (static_cast<REGISTER>(i) == clear_reg) {
                            continue;
                        }

                        const auto idx = index_for_var(bb, reg_map[i].current_var);
                        size_t next_use = 0;
                        for (size_t j = 0; j < var_map[idx].uses.size(); ++j) {
                            if (var_map[idx].uses[j] > cur_time) {
                                next_use = var_map[idx].uses[j];
                                break;
                            }
                        }
                        /*assert(next_use != 0);*/
                        if (next_use == 0) {
                            // this means this variable is needed in the current step
                            continue;
                        }

                        if (!last_used_var || next_use > last_use_time) {
                            last_used_var = reg_map[i].current_var;
                            last_use_time = next_use;
                            last_used_idx = idx;
                            reg = static_cast<REGISTER>(i);
                        }
                    }

                    if (last_used_var->type != Type::imm && !std::holds_alternative<size_t>(last_used_var->info)) {
                        // no need to save immediates or statics
                        fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", last_used_idx, reg_names[reg][0]);
                    }
                }
            }
        }

        auto* var = reg_map[reg].current_var;
        if (var != nullptr) {
            const auto idx = index_for_var(bb, var);
            auto& info = var_map[idx];
            if (info.last_used >= cur_time && var->type != Type::imm && !std::holds_alternative<size_t>(var->info)) {
                // TODO: see clear_reg
                fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", idx, reg_names[reg][0]);
            }

            info.cur_reg = REG_NONE;
        }

        reg_map[reg].current_var = nullptr;
        return reg;
    };

    const auto clear_reg = [&reg_map, &var_map, bb, this] (size_t cur_time, REGISTER reg) {
        if (reg_map[reg].current_var == nullptr) {
            return;
        }

        auto* var = reg_map[reg].current_var;
        const auto idx = index_for_var(bb, var);
        auto& info = var_map[idx];
        info.cur_reg = REG_NONE;
        if (info.last_used >= cur_time) {
            // TODO: this saves unneeded vars, also immediates and statics
            // but this is currently needed as we don't have fine-grained control over "time" in cfops
            // and when you need vars there they dont't get saved otherwise
            // for now maybe fix by using time - 1 in cfop context?
            fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", idx, reg_name(reg, var->type));
        }
        reg_map[reg].current_var = nullptr;
    };

    // loads a value into a register
    // TODO: this needs to be adjusted for the cfops where we can throw away registers if we wrote them out already
    const auto load_val_in_reg = [&reg_map, &var_map, bb, this, &alloc_reg, &clear_reg] (SSAVar* var, size_t cur_time, REGISTER only_this_reg = REG_NONE, REGISTER keep_clear_reg = REG_NONE) -> REGISTER {
        const auto idx = index_for_var(bb, var);
        if (var_map[idx].cur_reg != REG_NONE && var_map[idx].cur_reg != keep_clear_reg && (only_this_reg == REG_NONE || only_this_reg == var_map[idx].cur_reg)) {
            return var_map[idx].cur_reg;
        }

        if (only_this_reg != REG_NONE && var_map[idx].cur_reg != REG_NONE) {
            reg_map[var_map[idx].cur_reg].current_var = nullptr;
            clear_reg(cur_time, only_this_reg);
            fprintf(out_fd, "mov %s, %s\n", reg_names[only_this_reg][0], reg_names[var_map[idx].cur_reg][0]);
            reg_map[only_this_reg].current_var = var;
            var_map[idx].cur_reg = only_this_reg;
            return only_this_reg;
        }

        const auto reg = alloc_reg(cur_time, only_this_reg, keep_clear_reg);

        // load var
        reg_map[reg].current_var = var;
        var_map[idx].cur_reg = reg;
        if (var->type == Type::imm) {
            const auto& info = std::get<SSAVar::ImmInfo>(var->info);
            if (info.binary_relative) {
                fprintf(out_fd, "lea %s, [binary + %ld]\n", reg_names[reg][0], info.val);
            } else {
                fprintf(out_fd, "mov %s, %ld\n", reg_names[reg][0], info.val);
            }
        } else if (std::holds_alternative<size_t>(var->info)) {
            fprintf(out_fd, "mov %s, [s%zu]\n", reg_names[reg][0], std::get<size_t>(var->info));
        } else {
            fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", reg_name(reg, var->type), index_for_var(bb, var));
        }

        return reg;
    };

    // init time of last use
    for (size_t i = 0; i < bb->variables.size(); ++i) {
        auto* var = bb->variables[i].get();
        var_map[i].last_used = i;
        var_map[i].uses.reserve(var->ref_count);
        var_map[i].uses.push_back(i);

        for (size_t j = i + 1; j < bb->variables.size(); ++j) {
            auto* var2 = bb->variables[j].get();
            if (!std::holds_alternative<std::unique_ptr<Operation>>(var2->info)) {
                continue;
            }

            auto* op = std::get<std::unique_ptr<Operation>>(var2->info).get();
            for (const auto& in_var : op->in_vars) {
                if (in_var == var) {
                    var_map[i].uses.push_back(j);
                    var_map[i].last_used = j;
                }
            }
        }

        auto var_used = false;
        for (const auto& cf_op : bb->control_flow_ops) {
            for (const auto& in_var : cf_op.in_vars) {
                if (in_var == var) {
                    var_map[i].uses.push_back(bb->variables.size() + 1);
                    var_map[i].last_used = bb->variables.size() + 1;
                    var_used = true;
                    break;
                }
            }
            if (var_used) {
                break;
            }

            for (const auto& input : cf_op.target_inputs()) {
                if (input == var) {
                    var_map[i].uses.push_back(bb->variables.size() + 1);
                    var_map[i].last_used = bb->variables.size() + 1;
                    var_used = true;
                    break;
                }
            }
            if (var_used) {
                break;
            }
        }
    }

    // stack-frame
    fprintf(out_fd, "b%zu:\npush rbp\nmov rbp, rsp\nsub rsp, %zu\n", bb->id, bb->variables.size() * 8);
    fprintf(out_fd, "# SBRA\n"); // single-block register allocation
    fprintf(out_fd, "# Virt Start: %#lx\n# Virt End:  %#lx\n", bb->virt_start_addr, bb->virt_end_addr);

    // compile vars
    for (size_t i = 0; i < bb->variables.size(); ++i) {
        auto* var = bb->variables[i].get();

        if (var->type == Type::imm || std::holds_alternative<size_t>(var->info)) {
            // skip immediates and statics, we load them on-demand
            continue;
        }

        if (!std::holds_alternative<std::unique_ptr<Operation>>(var->info)) {
            // skip vars that depend on ops of other vars for example
            continue;
        }

        auto* op = std::get<std::unique_ptr<Operation>>(var->info).get();
        switch (op->type) {
            case Instruction::add:
            [[fallthrough]];
            case Instruction::sub:
            [[fallthrough]];
            case Instruction::_or:
            [[fallthrough]];
            case Instruction::_and:
            [[fallthrough]];
            case Instruction::_xor:
            [[fallthrough]];
            case Instruction::max:
            [[fallthrough]];
            case Instruction::min:
            [[fallthrough]];
            case Instruction::smax:
            [[fallthrough]];
            case Instruction::smin:
            [[fallthrough]];
            case Instruction::mul_l:
            [[fallthrough]];
            case Instruction::ssmul_h:
            [[fallthrough]];
            case Instruction::uumul_h:
            [[fallthrough]];
            case Instruction::div:
            [[fallthrough]];
            case Instruction::udiv:
            [[fallthrough]];
            case Instruction::shl:
            [[fallthrough]];
            case Instruction::shr:
            [[fallthrough]];
            case Instruction::sar:
            {
                // 3 cases, two immediates, one imm and one var, two vars
                auto* in1 = op->in_vars[0].get();
                auto* in2 = op->in_vars[1].get();
                if (in1->type == Type::imm && in2->type == Type::imm 
                    && !std::get<SSAVar::ImmInfo>(in1->info).binary_relative && !std::get<SSAVar::ImmInfo>(in2->info).binary_relative) {
                    // space for dst
                    const auto dst_reg = alloc_reg(i);
                    auto* dst = op->out_vars[0];
                    reg_map[dst_reg].current_var = dst;
                    var_map[index_for_var(bb, dst)].cur_reg = dst_reg;
                    uint64_t val1 = static_cast<uint64_t>(std::get<SSAVar::ImmInfo>(in1->info).val);
                    uint64_t val2 = static_cast<uint64_t>(std::get<SSAVar::ImmInfo>(in2->info).val);
                    uint64_t res = const_eval(op->type, val1, val2, dst->type);
                    if (dst->type == Type::i32) {
                        res = res & 0xFFFFFFFF;
                    } else if (dst->type == Type::i16) {
                        res = res & 0xFFFF;
                    } else if (dst->type == Type::i8) {
                        res = res & 0xFF;
                    }
                    fprintf(out_fd, "mov %s, %ld\n", reg_name(dst_reg, dst->type), res);

                    if (op->type == Instruction::div || op->type == Instruction::udiv && op->out_vars[1] != nullptr) {
                        // hack for div remainder
                        const auto dst_reg = alloc_reg(i);
                        auto* dst = op->out_vars[1];
                        reg_map[dst_reg].current_var = dst;
                        var_map[index_for_var(bb, dst)].cur_reg = dst_reg;

                        uint64_t val1 = static_cast<uint64_t>(std::get<SSAVar::ImmInfo>(in1->info).val);
                        uint64_t val2 = static_cast<uint64_t>(std::get<SSAVar::ImmInfo>(in2->info).val);

                        uint64_t res = 0;
                        if (op->type == Instruction::div) {
                            switch (op->out_vars[1]->type) {
                                case Type::i64:
                                    res = static_cast<int64_t>(val1) % static_cast<int64_t>(val2);
                                    break;
                                case Type::i32:
                                    res = static_cast<int32_t>(val1) % static_cast<int32_t>(val2);
                                    break;
                                case Type::i16:
                                    res = static_cast<int16_t>(val1) % static_cast<int16_t>(val2);
                                    break;
                                case Type::i8:
                                    res = static_cast<int8_t>(val1) % static_cast<int8_t>(val2);
                                    break;
                            }
                        } else {
                            switch (op->out_vars[1]->type) {
                                case Type::i64:
                                    res = val1 % val2;
                                    break;
                                case Type::i32:
                                    res = (val1 & 0xFFFFFFFF) % (val2 & 0xFFFFFFFF);
                                    break;
                                case Type::i16:
                                    res = (val1 & 0xFFFF) % (val2 & 0xFFFF);
                                    break;
                                case Type::i8:
                                    res = (val1 & 0xFF) % (val2 & 0xFF);
                                    break;
                            }
                        }
                        fprintf(out_fd, "mov %s, %ld\n", reg_name(dst_reg, dst->type), res);
                    }
                } else if (in1->type == Type::imm && in2->type != Type::imm && !std::get<SSAVar::ImmInfo>(in1->info).binary_relative
                            || in1->type != Type::imm && in2->type == Type::imm && !std::get<SSAVar::ImmInfo>(in2->info).binary_relative) {

                    if (in1->type == Type::imm) {
                        if (op->type != Instruction::sub && op->type != Instruction::sumul_h 
                        && op->type != Instruction::div && op->type != Instruction::udiv
                        && op->type != Instruction::shl && op->type != Instruction::shr && op->type != Instruction::sar) {
                            // just swap the operands since it doesn't make a difference
                            std::swap(in1, in2);
                        } else if (op->type == Instruction::sub) {
                            // treat this like -in2 + in1
                            const auto dst_reg = load_val_in_reg(in2, i);
                            auto imm = std::get<SSAVar::ImmInfo>(in1->info).val;
                            fprintf(out_fd, "neg %s\n", reg_name(dst_reg, in2->type));
                            if (imm != 0) {
                                fprintf(out_fd, "add %s, %ld\n", reg_name(dst_reg, in2->type), imm);
                            }
                            auto* dst = op->out_vars[0];
                            var_map[index_for_var(bb, in2)].cur_reg = REG_NONE;
                            reg_map[dst_reg].current_var = dst;
                            var_map[index_for_var(bb, dst)].cur_reg = dst_reg;
                            break;
                        } else if (op->type == Instruction::sumul_h) {
                            // allocate a reg for the immediate and multiply
                            // TODO: load values into xmm register and sign extend the first one, then multiply and get the upper 64bit
                        } else if (op->type == Instruction::div || op->type == Instruction::udiv) {
                            // TODO: merge this with the two var form?
                            // TODO: load value into rax,rdx from the beginning if the values aren't currently loaded somewhere
                            const auto imm_reg = load_val_in_reg(in1, i);
                            assert(0);
                            exit(1);
                        } else {
                            assert(0);
                            exit(1);
                        }
                    }

                    // in1 is normal var, in2 is imm
                    const auto in1_idx = index_for_var(bb, in1);
                    REGISTER dst_reg = REG_NONE;
                    if (op->type != Instruction::div && op->type != Instruction::udiv
                        && op->type != Instruction::ssmul_h && op->type != Instruction::uumul_h) {
                        dst_reg = load_val_in_reg(in1, i);
                    } else {
                        // need to load into rax and clear rdx
                        dst_reg = load_val_in_reg(in1, i, REG_A, REG_D);

                        clear_reg(i, REG_D);
                        if (op->type == Instruction::div && op->type == Instruction::udiv) {
                            fprintf(out_fd, "cqo\n");
                        }
                    }
                    const auto dst_reg_name = reg_name(dst_reg, in1->type);
                    const auto imm = std::get<SSAVar::ImmInfo>(in2->info).val;
                    if (var_map[in1_idx].last_used > i && !std::holds_alternative<size_t>(in1->info)) {
                        // backup in1 if it is not a static
                        // TODO: mark value if it has already been saved once
                        fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", in1_idx, dst_reg_name);
                    }

                    switch (op->type) {
                        case Instruction::add:
                            fprintf(out_fd, "add %s, %ld\n", dst_reg_name, imm);
                            break;
                        case Instruction::sub:
                            fprintf(out_fd, "sub %s, %ld\n", dst_reg_name, imm);
                            break;
                        case Instruction::mul_l:
                            {
                                if (static_cast<uint64_t>(imm) <= 0xFFFFFFFF) {
                                    fprintf(out_fd, "imul %s, %ld\n", dst_reg_name, imm);
                                } else {
                                    // need to load the immediate into a register since imul only allows a 32bit immediate
                                    const auto imm_reg = load_val_in_reg(in2, i);
                                    fprintf(out_fd, "imul %s, %s\n", dst_reg_name, reg_name(imm_reg, in1->type));
                                }
                                break;
                            }
                        case Instruction::ssmul_h:
                        {
                            const auto imm_reg = load_val_in_reg(in2, i, REG_D);
                            fprintf(out_fd, "imul %s\n", reg_name(imm_reg, in1->type));
                            break;
                        }
                        case Instruction::uumul_h:
                        {
                            const auto imm_reg = load_val_in_reg(in2, i, REG_D);
                            fprintf(out_fd, "mul %s\n", reg_name(imm_reg, in1->type));
                            break;
                        }
                        case Instruction::div:
                            {
                                const auto imm_reg = load_val_in_reg(in2, i, REG_NONE, REG_D);
                                fprintf(out_fd, "idiv %s\n", reg_name(imm_reg, in1->type));
                                break;
                            }
                        case Instruction::udiv:
                            {
                                const auto imm_reg = load_val_in_reg(in2, i, REG_NONE, REG_D);
                                fprintf(out_fd, "div %s\n", reg_name(imm_reg, in1->type));
                                break;
                            }
                        case Instruction::shl:
                            assert(imm >= 0 && imm <= 64);
                            fprintf(out_fd, "shl %s, %ld\n", dst_reg_name, imm);
                            break;
                        case Instruction::shr:
                            assert(imm >= 0 && imm <= 64);
                            fprintf(out_fd, "shr %s, %ld\n", dst_reg_name, imm);
                            break;
                        case Instruction::sar:
                            assert(imm >= 0 && imm <= 64);
                            fprintf(out_fd, "sar %s, %ld\n", dst_reg_name, imm);
                            break;
                        case Instruction::_or:
                            fprintf(out_fd, "or %s, %ld\n", dst_reg_name, imm);
                            break;
                        case Instruction::_and:
                            fprintf(out_fd, "and %s, %ld\n", dst_reg_name, imm);
                            break;
                        case Instruction::_xor:
                            fprintf(out_fd, "xor %s, %ld\n", dst_reg_name, imm);
                            break;
                        case Instruction::max:
                            fprintf(out_fd, "cmp %s, %ld\n", dst_reg_name, imm);
                            fprintf(out_fd, "jae b%zu_%zu_max\n", bb->id, i);
                            fprintf(out_fd, "mov %s, %ld\n", dst_reg_name, imm);
                            fprintf(out_fd, "b%zu_%zu_max:\n", bb->id, i);
                            break;
                        case Instruction::min:
                            fprintf(out_fd, "cmp %s, %ld\n", dst_reg_name, imm);
                            fprintf(out_fd, "jbe b%zu_%zu_min\n", bb->id, i);
                            fprintf(out_fd, "mov %s, %ld\n", dst_reg_name, imm);
                            fprintf(out_fd, "b%zu_%zu_min:\n", bb->id, i);
                            break;
                        case Instruction::smax:
                            fprintf(out_fd, "cmp %s, %ld\n", dst_reg_name, imm);
                            fprintf(out_fd, "jge b%zu_%zu_smax\n", bb->id, i);
                            fprintf(out_fd, "mov %s, %ld\n", dst_reg_name, imm);
                            fprintf(out_fd, "b%zu_%zu_smax:\n", bb->id, i);
                            break;
                        case Instruction::smin:
                            fprintf(out_fd, "cmp %s, %ld\n", dst_reg_name, imm);
                            fprintf(out_fd, "jle b%zu_%zu_smin\n", bb->id, i);
                            fprintf(out_fd, "mov %s, %ld\n", dst_reg_name, imm);
                            fprintf(out_fd, "b%zu_%zu_smin:\n", bb->id, i);
                            break;
                    }

                    if (op->type != Instruction::div && op->type != Instruction::udiv
                        && op->type != Instruction::ssmul_h && op->type != Instruction::uumul_h) {
                        auto* dst = op->out_vars[0];
                        var_map[in1_idx].cur_reg = REG_NONE;
                        reg_map[dst_reg].current_var = dst;
                        var_map[index_for_var(bb, dst)].cur_reg = dst_reg;
                    } else if (op->type == Instruction::div || op->type == Instruction::udiv) {
                        assert(dst_reg == REG_A);
                        auto* dst = op->out_vars[0];
                        var_map[in1_idx].cur_reg = REG_NONE;
                        reg_map[REG_A].current_var = dst;
                        var_map[index_for_var(bb, dst)].cur_reg = REG_A;

                        if (op->out_vars[1]) {
                            assert(reg_map[REG_D].current_var == nullptr);
                            auto* dst = op->out_vars[1];
                            reg_map[REG_D].current_var = dst;
                            var_map[index_for_var(bb, dst)].cur_reg = REG_D;
                        }
                    } else {
                        // result is in rdx but rax is clobbered, too
                        auto* dst = op->out_vars[0];
                        // clear register mapping for in2 and in1
                        var_map[index_for_var(bb, in2)].cur_reg = REG_NONE;
                        reg_map[REG_A].current_var = nullptr;
                        var_map[in1_idx].cur_reg = REG_NONE;

                        reg_map[REG_D].current_var = dst;
                        var_map[index_for_var(bb, dst)].cur_reg = REG_D;
                    }
                } else {
                    assert(in1->type == in2->type);
                    // two normal vars or binary relative immediates
                    REGISTER in2_reg = REG_NONE, dst_reg = REG_NONE;
                    if (op->type != Instruction::div && op->type != Instruction::udiv) {
                        if (op->type == Instruction::shl || op->type == Instruction::shr || op->type == Instruction::sar) {
                            in2_reg = load_val_in_reg(in2, i, REG_C);
                        } else {
                            in2_reg = load_val_in_reg(in2, i);
                        }
                        dst_reg = load_val_in_reg(in1, i);
                    } else {
                        dst_reg = load_val_in_reg(in1, i, REG_A);
                        in2_reg = load_val_in_reg(in2, i, REG_NONE, REG_D);

                        clear_reg(i, REG_D);
                        fprintf(out_fd, "cqo\n");
                    }
                    const auto in1_idx = index_for_var(bb, in1);
                    auto type = in1->type;
                    if (in1->type == Type::imm && in2->type == Type::imm) {
                        type = Type::i64;
                    } else if (in1->type == Type::imm) {
                        type = in2->type;
                    }
                    const auto dst_reg_name = reg_name(dst_reg, type);
                    const auto in2_reg_name = reg_name(in2_reg, type);

                    if (var_map[in1_idx].last_used > i && !std::holds_alternative<size_t>(in1->info) && in1->type != Type::imm) {
                        // backup in1 if it is not a static or immediate
                        // TODO: mark value if it has already been saved once
                        fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", in1_idx, dst_reg_name);
                    }

                    switch (op->type) {
                        case Instruction::add:
                            fprintf(out_fd, "add %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::sub:
                            fprintf(out_fd, "sub %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::mul_l:
                            fprintf(out_fd, "imul %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::div:
                            fprintf(out_fd, "idiv %s\n", in2_reg_name);
                            break;
                        case Instruction::udiv:
                            fprintf(out_fd, "div %s\n", in2_reg_name);
                            break;
                        case Instruction::shl:
                            fprintf(out_fd, "shl %s, cl\n", dst_reg_name);
                            break;
                        case Instruction::shr:
                            fprintf(out_fd, "shr %s, cl\n", dst_reg_name);
                            break;
                        case Instruction::sar:
                            fprintf(out_fd, "sar %s, cl\n", dst_reg_name);
                            break;
                        case Instruction::_or:
                            fprintf(out_fd, "or %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::_and:
                            fprintf(out_fd, "and %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::_xor:
                            fprintf(out_fd, "xor %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::max:
                            fprintf(out_fd, "cmp %s, %s\n", dst_reg_name, in2_reg_name);
                            fprintf(out_fd, "cmovb %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::min:
                            fprintf(out_fd, "cmp %s, %s\n", dst_reg_name, in2_reg_name);
                            fprintf(out_fd, "cmova %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::smax:
                            fprintf(out_fd, "cmp %s, %s\n", dst_reg_name, in2_reg_name);
                            fprintf(out_fd, "cmovl %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                        case Instruction::smin:
                            fprintf(out_fd, "cmp %s, %s\n", dst_reg_name, in2_reg_name);
                            fprintf(out_fd, "cmovg %s, %s\n", dst_reg_name, in2_reg_name);
                            break;
                    }

                    if (op->type != Instruction::div && op->type != Instruction::udiv) {
                        auto* dst = op->out_vars[0];
                        var_map[in1_idx].cur_reg = REG_NONE;
                        reg_map[dst_reg].current_var = dst;
                        var_map[index_for_var(bb, dst)].cur_reg = dst_reg;
                    } else {
                        assert(dst_reg == REG_A);
                        if (op->out_vars[0]) {
                            auto* dst = op->out_vars[0];
                            var_map[in1_idx].cur_reg = REG_NONE;
                            reg_map[REG_A].current_var = dst;
                            var_map[index_for_var(bb, dst)].cur_reg = REG_A;
                        }

                        if (op->out_vars[1]) {
                            assert(reg_map[REG_D].current_var == nullptr);
                            auto* dst = op->out_vars[1];
                            reg_map[REG_D].current_var = dst;
                            var_map[index_for_var(bb, dst)].cur_reg = REG_D;
                        }
                    }
                }
            }
            break;
            case Instruction::sumul_h:
                assert(0);
                exit(1);
            case Instruction::cast:
            [[fallthrough]];
            case Instruction::sign_extend:
            [[fallthrough]];
            case Instruction::zero_extend:
            {
                auto* input = op->in_vars[0].get();
                auto* output = op->out_vars[0];
                if (input->type == Type::imm && !std::get<SSAVar::ImmInfo>(input->info).binary_relative) {
                    // cast,sign_extend and zero_extend are no-ops
                    auto imm = std::get<SSAVar::ImmInfo>(input->info).val;
                    switch (output->type) {
                        case Type::i64:
                            break;
                        case Type::i32:
                            imm = imm & 0xFFFFFFFF;
                            break;
                        case Type::i16:
                            imm = imm & 0xFFFF;
                            break;
                        case Type::i8:
                            imm = imm & 0xFF;
                            break;
                        default:
                            assert(0);
                            exit(1);
                    }
                    const auto dst_reg = alloc_reg(i);
                    const auto dst_reg_name = reg_name(dst_reg, output->type);
                    fprintf(out_fd, "mov %s, %ld\n", dst_reg_name, imm);

                    reg_map[dst_reg].current_var = output;
                    var_map[index_for_var(bb, output)].cur_reg = dst_reg;
                    break;
                }

                const auto dst_reg = load_val_in_reg(input, i);
                const auto dst_reg_name = reg_name(dst_reg, input->type);
                const auto in_idx = index_for_var(bb, input);
                auto& info = var_map[in_idx];
                if (info.last_used > i && !std::holds_alternative<size_t>(input->info) && input->type != Type::imm) {
                    fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", in_idx, dst_reg_name);
                }

                // TODO: in theory you could simply alias the input var for cast and zero_extend
                if (op->type == Instruction::sign_extend) {
                    fprintf(out_fd, "movsx %s, %s\n", reg_name(dst_reg, output->type), dst_reg_name);
                } else if (input->type != output->type) {
                    // clear upper parts of register
                    // find smallest type
                    auto type = input->type;
                    if (output->type == Type::i8) {
                        type = Type::i8;
                    } else if (output->type == Type::i16 && type != Type::i8) {
                        type = Type::i16;
                    } else if (output->type == Type::i32 && type != Type::i8 && type != Type::i16) {
                        type = Type::i32;
                    }
                    if (type == Type::i32) {
                        const auto name = reg_name(dst_reg, type);
                        fprintf(out_fd, "mov %s, %s\n", name, name);
                    } else {
                        if (type == Type::i16) {
                            fprintf(out_fd, "and %s, 0xFFFF\n", reg_names[dst_reg][0]);
                        } else if (type == Type::i8) {
                            fprintf(out_fd, "and %s, 0xFF\n", reg_names[dst_reg][0]);
                        }
                        // nothing to do for 64 bit
                    }
                }

                info.cur_reg = REG_NONE;
                reg_map[dst_reg].current_var = output;
                var_map[index_for_var(bb, output)].cur_reg = dst_reg;
                break;
            }
            case Instruction::load: {
                auto* addr = op->in_vars[0].get();
                auto* dst = op->out_vars[0];
                assert(addr->type == Type::imm || addr->type == Type::i64);
                const auto dst_reg = load_val_in_reg(addr, i);

                auto& info = var_map[index_for_var(bb, addr)];
                if (info.last_used > i && addr->type != Type::imm && !std::holds_alternative<size_t>(addr->info)) {
                    fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", reg_names[dst_reg][0]);
                }

                fprintf(out_fd, "mov %s, [%s]\n", reg_name(dst_reg, dst->type), reg_names[dst_reg][0]);

                info.cur_reg = REG_NONE;
                reg_map[dst_reg].current_var = dst;
                var_map[index_for_var(bb, dst)].cur_reg = dst_reg;
                break;
            }
            case Instruction::store: {
                auto* addr = op->in_vars[0].get();
                auto* val = op->in_vars[1].get();
                assert(addr->type == Type::imm || addr->type == Type::i64);
                const auto addr_reg = load_val_in_reg(addr, i);
                // TODO: for up to 32bit immediates you can use a mov with an immediate operand
                const auto val_reg = load_val_in_reg(val, i, REG_NONE, addr_reg);

                fprintf(out_fd, "mov [%s], %s\n", reg_names[addr_reg][0], reg_name(val_reg, val->type));
                break;
            }
            case Instruction::setup_stack: {
                auto* dst = op->out_vars[0];
                const auto dst_reg = alloc_reg(i);

                fprintf(out_fd, "mov %s, [init_stack_ptr]\n", reg_names[dst_reg][0]);

                reg_map[dst_reg].current_var = dst;
                var_map[index_for_var(bb, dst)].cur_reg = dst_reg;
                break;
            }
            case Instruction::slt:
            [[fallthrough]];
            case Instruction::sltu: {
                auto* cmp1 = op->in_vars[0].get();
                auto* cmp2 = op->in_vars[1].get();
                assert(cmp1->type == cmp2->type || cmp1->type == Type::imm || cmp2->type == Type::imm);
                auto* val1 = op->in_vars[2].get();
                auto* val2 = op->in_vars[3].get();
                auto* dst = op->out_vars[0];

                // TODO: optimize for the case that val1 = 1, val2 = 0
                const auto cmp1_reg = load_val_in_reg(cmp1, i);
                const auto val1_reg = load_val_in_reg(val1, i);
                const auto val2_reg = load_val_in_reg(val2, i);
                if (cmp2->type == Type::imm && !std::get<SSAVar::ImmInfo>(cmp2->info).binary_relative) {
                    fprintf(out_fd, "cmp %s, %ld\n", reg_name(cmp1_reg, cmp1->type), std::get<SSAVar::ImmInfo>(cmp2->info).val);
                } else {
                    const auto cmp2_reg = load_val_in_reg(cmp2, i);
                    auto type = cmp1->type;
                    if (type == Type::imm) {
                        type = cmp2->type;
                    }
                    if (type == Type::imm) {
                        type = Type::i64;
                    }
                    fprintf(out_fd, "cmp %s, %s\n", reg_name(cmp1_reg, type), reg_name(cmp2_reg, type));
                }

                if (op->type == Instruction::slt) {
                    fprintf(out_fd, "cmovl %s, %s\n", reg_name(cmp1_reg, dst->type), reg_name(val1_reg, dst->type));
                    fprintf(out_fd, "cmovge %s, %s\n", reg_name(cmp1_reg, dst->type), reg_name(val2_reg, dst->type));
                } else {
                    fprintf(out_fd, "cmovb %s, %s\n", reg_name(cmp1_reg, dst->type), reg_name(val1_reg, dst->type));
                    fprintf(out_fd, "cmovae %s, %s\n", reg_name(cmp1_reg, dst->type), reg_name(val2_reg, dst->type));
                }

                reg_map[cmp1_reg].current_var = dst;
                var_map[index_for_var(bb, cmp1)].cur_reg = REG_NONE;
                var_map[index_for_var(bb, dst)].cur_reg = cmp1_reg;
                break;
            }
            default:
                break;
        }
    }

    // TODO: cfops
    const auto time = bb->variables.size() /*+ 1*/;
    // TODO: cfops need proper time management though that is rather complicated i think

    std::vector<std::vector<SSAVar*>> static_mapping = {};
    std::vector<bool> statics_written_out = {};
    statics_written_out.resize(bb->ir->statics.size());
    for (size_t i = 0; i < bb->control_flow_ops.size(); ++i) {
        static_mapping.emplace_back();
        static_mapping.back().resize(bb->ir->statics.size());
    }

    for (size_t i = 0; i < bb->control_flow_ops.size(); ++i) {
        const auto store_mapping = [&static_mapping, i](const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping) {
            for (const auto& pair : mapping) {
                static_mapping[i][pair.second] = pair.first.get();
            }
        };

        const auto& cf_op = bb->control_flow_ops[i];
        const auto store_target_inputs = [&static_mapping, i, &cf_op] (const std::vector<RefPtr<SSAVar>> &inputs) {
            const auto& target_inputs = cf_op.target()->inputs;
            assert(inputs.size() == target_inputs.size());

            for (size_t j = 0; j < target_inputs.size(); ++j) {
                auto* input = target_inputs[j];
                if (!std::holds_alternative<size_t>(input->info)) {
                    continue;
                }

                const auto static_idx = std::get<size_t>(input->info);
                static_mapping[i][static_idx] = inputs[j].get();
            }
        };

        switch (cf_op.type) {
            case CFCInstruction::syscall:
                store_mapping(std::get<CfOp::SyscallInfo>(cf_op.info).continuation_mapping);
                break;
            case CFCInstruction::ijump:
                store_mapping(std::get<CfOp::IJumpInfo>(cf_op.info).mapping);
                break;
            case CFCInstruction::jump:
                store_target_inputs(std::get<CfOp::JumpInfo>(cf_op.info).target_inputs);
                break;
            case CFCInstruction::cjump:
                store_target_inputs(std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs);
                break;
            case CFCInstruction::unreachable:
                break;
            default:
                assert(0);
                exit(1);
        }
    }

    if (!static_mapping.empty() && false) {
        for (size_t i = 0; i < bb->ir->statics.size(); ++i) {
            auto* var = static_mapping[0][i];
            if (!var || var->type == Type::mt) {
                continue;
            }

            auto shared = true;
            for (size_t j = 0; j < static_mapping.size(); ++j) {
                if (var != static_mapping[j][i]) {
                    shared = false;
                    break;
                }
            }
            if (!shared) {
                continue;
            }

            statics_written_out[i] = true;
            if (std::holds_alternative<size_t>(var->info) && std::get<size_t>(var->info) == i) {
                continue;
            }

            if (var->type == Type::imm) {
                const auto& info = std::get<SSAVar::ImmInfo>(var->info);
                if (!info.binary_relative && static_cast<uint64_t>(info.val) <= 0xFFFFFFFF) {
                    fprintf(out_fd, "mov %s [s%zu], %ld\n", mem_size(bb->ir->statics[i].type), i, info.val);
                    continue;
                }

                const auto reg = load_val_in_reg(var, time);
                fprintf(out_fd, "mov [s%zu], %s\n", i, reg_names[reg][0]);
            } else {
                const auto reg = load_val_in_reg(var, time);
                fprintf(out_fd, "mov [s%zu], %s\n", i, reg_name(reg, var->type));
            }
        }
    }

    // need to save otherwise later cfops might assume that a value is in a register when it really isnt
    // TODO: this can be solved quite more intelligently
    const auto var_map_bak = var_map;
    const auto reg_map_bak = reg_map;
    for (size_t i = 0; i < bb->control_flow_ops.size(); ++i) {
        const auto& cf_op = bb->control_flow_ops[i];
        if (i != 0) {
            var_map = var_map_bak;
            reg_map = reg_map_bak;
        }

        fprintf(out_fd, "b%zu_cf%zu:\n", bb->id, i);
        if (cf_op.type == CFCInstruction::cjump) {
            // condition
            auto* cmp1 = cf_op.in_vars[0].get();
            auto* cmp2 = cf_op.in_vars[1].get();
            const auto cmp1_reg = load_val_in_reg(cmp1, time);
            const auto cmp2_reg = load_val_in_reg(cmp2, time);
            auto cmp1_type = cmp1->type;
            auto cmp2_type = cmp2->type;
            if (cmp1_type == cmp2_type && cmp1_type == Type::imm) {
                cmp1_type = cmp2_type = Type::i64;
            } else if (cmp1_type == Type::imm) {
                cmp1_type = cmp2_type;
            } else if (cmp2_type == Type::imm) {
                cmp2_type = cmp1_type;
            } else {
                assert(cmp1_type == cmp2_type);
            }

            const auto cmp1_reg_name = reg_name(cmp1_reg, cmp1_type);
            const auto cmp2_reg_name = reg_name(cmp2_reg, cmp2_type);
            fprintf(out_fd, "cmp %s, %s\n", cmp1_reg_name, cmp2_reg_name);

            const auto &info = std::get<CfOp::CJumpInfo>(cf_op.info);
            switch (info.type) {
            case CfOp::CJumpInfo::CJumpType::eq:
                fprintf(out_fd, "jne b%zu_cf%zu\n", bb->id, i + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::neq:
                fprintf(out_fd, "je b%zu_cf%zu\n", bb->id, i + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::lt:
                fprintf(out_fd, "jae b%zu_cf%zu\n", bb->id, i + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::gt:
                fprintf(out_fd, "jbe b%zu_cf%zu\n", bb->id, i + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::slt:
                fprintf(out_fd, "jge b%zu_cf%zu\n", bb->id, i + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::sgt:
                fprintf(out_fd, "jle b%zu_cf%zu\n", bb->id, i + 1);
                break;
            }
        }

        const auto write_cont_mapping = [&statics_written_out, this, &load_val_in_reg, time, bb] (const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping) {
            for (const auto& pair : mapping) {
                if (statics_written_out[pair.second]) {
                    continue;
                }

                auto* var = pair.first.get();
                if (var->type == Type::mt) {
                    continue;
                }

                if (std::holds_alternative<size_t>(var->info) && std::get<size_t>(var->info) == pair.second) {
                    continue;
                }

                if (var->type == Type::imm) {
                    const auto& info = std::get<SSAVar::ImmInfo>(var->info);
                    if (!info.binary_relative && static_cast<uint64_t>(info.val) <= 0xFFFFFFFF) {
                        fprintf(out_fd, "mov %s [s%zu], %ld\n",  mem_size(bb->ir->statics[pair.second].type),pair.second, info.val);
                        continue;
                    }

                    const auto reg = load_val_in_reg(var, time);
                    fprintf(out_fd, "mov [s%zu], %s\n", pair.second, reg_names[reg][0]);
                } else {
                    const auto reg = load_val_in_reg(var, time);
                    fprintf(out_fd, "mov [s%zu], %s\n", pair.second, reg_name(reg, var->type));
                }
            }
        };

        const auto write_input_mapping = [&statics_written_out, this, &load_val_in_reg, time, &cf_op, bb] (const std::vector<RefPtr<SSAVar>> &inputs) {
            const auto& target_inputs = cf_op.target()->inputs;
            assert(inputs.size() == target_inputs.size());

            for (size_t j = 0; j < target_inputs.size(); ++j) {
                auto* input = target_inputs[j];
                if (!std::holds_alternative<size_t>(input->info)) {
                    continue;
                }

                const auto static_idx = std::get<size_t>(input->info);
                if (statics_written_out[static_idx]) {
                    continue;
                }

                // TODO: dupe code
                auto* var = inputs[j].get();
                if (var->type == Type::mt) {
                    continue;
                }

                if (std::holds_alternative<size_t>(var->info) && std::get<size_t>(var->info) == static_idx) {
                    continue;
                }

                if (var->type == Type::imm) {
                    const auto& info = std::get<SSAVar::ImmInfo>(var->info);
                    if (!info.binary_relative && static_cast<uint64_t>(info.val) <= 0xFFFFFFFF) {
                        fprintf(out_fd, "mov %s [s%zu], %ld\n",  mem_size(bb->ir->statics[static_idx].type),static_idx, info.val);
                        continue;
                    }

                    const auto reg = load_val_in_reg(var, time);
                    fprintf(out_fd, "mov [s%zu], %s\n", static_idx, reg_names[reg][0]);
                } else {
                    const auto reg = load_val_in_reg(var, time);
                    fprintf(out_fd, "mov [s%zu], %s\n", static_idx, reg_name(reg, var->type));
                }
            }
        };

        // write out statics
        switch (cf_op.type) {
            case CFCInstruction::syscall:
                write_cont_mapping(std::get<CfOp::SyscallInfo>(cf_op.info).continuation_mapping);
                break;
            case CFCInstruction::ijump:
                write_cont_mapping(std::get<CfOp::IJumpInfo>(cf_op.info).mapping);
                break;
            case CFCInstruction::jump:
                write_input_mapping(std::get<CfOp::JumpInfo>(cf_op.info).target_inputs);
                break;
            case CFCInstruction::cjump:
                write_input_mapping(std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs);
                break;
            case CFCInstruction::unreachable:
                break;
            default:
                assert(0);
                exit(1);
        }

        switch (cf_op.type) {
            case CFCInstruction::jump:
            [[fallthrough]];
            case CFCInstruction::cjump:
                fprintf(out_fd, "# destroy stack space\n");
                fprintf(out_fd, "mov rsp, rbp\npop rbp\n");
                fprintf(out_fd, "jmp b%zu\n", cf_op.target()->id);
                break;
            case CFCInstruction::unreachable:
                err_msgs.emplace_back(ErrType::unreachable, bb);
                fprintf(out_fd, "lea rdi, [rip + err_unreachable_b%zu]\n", bb->id);
                fprintf(out_fd, "jmp panic\n");
                break;
            case CFCInstruction::ijump:
            {
                // TODO: we get a problem if the dst is in a static that has already been written out (so overwritten)
                auto* dst = cf_op.in_vars[0].get();
                const auto dst_reg = load_val_in_reg(dst, time, REG_A);
                const auto dst_reg_name = reg_names[dst_reg][0];
                assert(dst->type == Type::imm || dst->type == Type::i64);
                fprintf(out_fd, "# destroy stack space\n");
                fprintf(out_fd, "mov rsp, rbp\npop rbp\n");

                err_msgs.emplace_back(ErrType::unresolved_ijump, bb);

                /* we trust the lifter that the ijump destination is already aligned */

                /* turn absolute address into relative offset from start of first basicblock */
                fprintf(out_fd, "sub rax, %zu\n", ir->virt_bb_start_addr);

                fprintf(out_fd, "cmp rax, ijump_lookup_end - ijump_lookup\n");
                fprintf(out_fd, "ja 0f\n");
                fprintf(out_fd, "lea rdi, [rip + ijump_lookup]\n");
                fprintf(out_fd, "mov rdi, [rdi + 4 * rax]\n");
                fprintf(out_fd, "test rdi, rdi\n");
                fprintf(out_fd, "je 0f\n");
                fprintf(out_fd, "jmp rdi\n");
                fprintf(out_fd, "0:\n");
                fprintf(out_fd, "lea rdi, [rip + err_unresolved_ijump_b%zu]\n", bb->id);
                fprintf(out_fd, "jmp panic\n");
                break;
            }
            case CFCInstruction::syscall: {
                const auto& info = std::get<CfOp::SyscallInfo>(cf_op.info);
                for (size_t i = 0; i < call_reg.size(); ++i) {
                    if (cf_op.in_vars[i] == nullptr)
                        break;

                    if (cf_op.in_vars[i]->type == Type::mt)
                        continue;

                    load_val_in_reg(cf_op.in_vars[i].get(), time, call_reg[i]);
                }
                if (cf_op.in_vars[6] == nullptr) {
                    fprintf(out_fd, "push 0\n");
                } else {
                    // TODO: clear rax before when we have inputs < 64 bit
                    load_val_in_reg(cf_op.in_vars[6].get(), time, REG_A);
                    fprintf(out_fd, "push rax\n");
                }

                fprintf(out_fd, "call syscall_impl\n");
                if (info.static_mapping.size() > 0) {
                    fprintf(out_fd, "mov [s%zu], rax\n", info.static_mapping.at(0));
                    if (info.static_mapping.size() == 2) {
                        fprintf(out_fd, "mov [s%zu], rdx\n", info.static_mapping.at(1));
                    } else {
                        // syscalls only return max 2 values
                        assert(0);
                    }
                }
                fprintf(out_fd, "# destroy stack space\n");
                fprintf(out_fd, "mov rsp, rbp\npop rbp\n");
                fprintf(out_fd, "jmp b%zu\n", info.continuation_block->id);
                break;
            }
        }
    }
}