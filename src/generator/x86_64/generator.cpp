#include "generator/x86_64/generator.h"

using namespace generator::x86_64;

// TODO: imm handling is questionable at best here

namespace {
std::array<const char *, 4> op_reg_mapping_64 = {"rax", "rbx", "rcx", "rdx"};

std::array<const char *, 4> op_reg_mapping_32 = {"eax", "ebx", "ecx", "edx"};

std::array<const char *, 4> op_reg_mapping_16 = {"ax", "bx", "cx", "dx"};

std::array<const char *, 4> op_reg_mapping_8 = {"al", "bl", "cl", "dl"};

std::array<const char *, 4> &op_reg_map_for_type(const Type type) {
    switch (type) {
    case Type::imm:
    case Type::i64:
        return op_reg_mapping_64;
    case Type::i32:
        return op_reg_mapping_32;
    case Type::i16:
        return op_reg_mapping_16;
    case Type::i8:
        return op_reg_mapping_8;
    case Type::f64:
    case Type::f32:
    case Type::mt:
        break;
    }

    assert(0);
    exit(1);
}

std::array<const char *, 6> call_reg = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

const char *rax_from_type(const Type type) {
    const auto *reg_str = "rax";
    switch (type) {
    case Type::imm:
    case Type::i64:
        break;
    case Type::i32:
        reg_str = "eax";
        break;
    case Type::i16:
        reg_str = "ax";
        break;
    case Type::i8:
        reg_str = "al";
        break;
    case Type::f32:
    case Type::f64:
    case Type::mt:
        assert(0);
        exit(1);
    }

    return reg_str;
}

const char *ptr_from_type(const Type type) {
    const auto *ptr_type = "qword ptr";
    switch (type) {
    case Type::imm:
    case Type::i64:
        break;
    case Type::i32:
        ptr_type = "dword ptr";
        break;
    case Type::i16:
        ptr_type = "word ptr";
        break;
    case Type::i8:
        ptr_type = "byte ptr";
        break;
    case Type::f32:
    case Type::f64:
    case Type::mt:
        assert(0);
        exit(1);
    }
    return ptr_type;
}

size_t index_for_var(const BasicBlock *block, const SSAVar *var) {
    for (size_t idx = 0; idx < block->variables.size(); ++idx) {
        if (block->variables[idx].get() == var)
            return idx;
    }

    assert(0);
    exit(1);
}
} // namespace

void Generator::compile() {
    assert(err_msgs.empty());

    fprintf(out_fd, ".intel_syntax noprefix\n\n");
    if (!binary_filepath.empty()) {
        compile_section(Section::DATA);
        fprintf(out_fd, "binary: .incbin \"%s\"\n", binary_filepath.c_str());
    }

    compile_statics();

    compile_section(Section::BSS);

    fprintf(out_fd, "param_passing:\n");
    fprintf(out_fd, ".space 128\n");
    fprintf(out_fd, ".type param_passing,STT_OBJECT\n");
    fprintf(out_fd, ".size param_passing,$-param_passing\n");

    fprintf(out_fd, ".align 16\n");
    fprintf(out_fd, "stack_space:\n");
    fprintf(out_fd, ".space 1048576\n"); /* 1MiB */
    fprintf(out_fd, "stack_space_end:\n");
    fprintf(out_fd, ".type stack_space,STT_OBJECT\n");
    fprintf(out_fd, ".size stack_space,$-stack_space\n");

    fprintf(out_fd, "init_stack_ptr: .quad 0\n");

    compile_blocks();

    compile_entry();

    // TODO: does this section name need to be smth else?
    compile_section(Section::RODATA);
    compile_err_msgs();
}

void Generator::compile_statics() {
    compile_section(Section::DATA);

    for (const auto &var : ir->statics) {
        std::fprintf(out_fd, "s%zu: .quad 0\n", var.id); // for now have all of the statics be 64bit
    }
}

void Generator::compile_blocks() {
    compile_section(Section::TEXT);

    for (auto &block : ir->basic_blocks) {
        compile_block(block.get());
    }
}

void Generator::compile_block(const BasicBlock *block) {
    for (const auto *input : block->inputs) {
        // don't try to compile blocks that cannot be independent for now
        if (!std::holds_alternative<size_t>(input->info))
            return;
    }

    const size_t stack_size = block->variables.size() * 8;
    fprintf(out_fd, "b%zu:\npush rbp\nmov rbp, rsp\nsub rsp, %zu\n", block->id, stack_size);
    compile_vars(block);

    for (size_t i = 0; i < block->control_flow_ops.size(); ++i) {
        const auto &cf_op = block->control_flow_ops[i];
        assert(cf_op.source == block);

        fprintf(out_fd, "b%zu_cf%zu:\n", block->id, i);
        switch (cf_op.type) {
        case CFCInstruction::jump:
            compile_cf_args(block, cf_op);
            fprintf(out_fd, "# control flow\n");
            fprintf(out_fd, "jmp b%zu\n", std::get<CfOp::JumpInfo>(cf_op.info).target->id);
            break;
        case CFCInstruction::_return:
            compile_ret_args(block, cf_op);
            fprintf(out_fd, "# control flow\nret\n");
            break;
        case CFCInstruction::cjump:
            compile_cjump(block, cf_op, i);
            break;
        case CFCInstruction::call:
            compile_continuation_args(block, std::get<CfOp::CallInfo>(cf_op.info).continuation_mapping);
            compile_cf_args(block, cf_op);
            fprintf(out_fd, "# control flow\n");
            fprintf(out_fd, "call b%zu\n", std::get<CfOp::CallInfo>(cf_op.info).target->id);
            assert(std::get<CfOp::CallInfo>(cf_op.info).continuation_block != nullptr);
            fprintf(out_fd, "jmp b%zu\n", std::get<CfOp::CallInfo>(cf_op.info).continuation_block->id);
            break;
        case CFCInstruction::syscall:
            compile_syscall(block, cf_op);
            break;
        case CFCInstruction::unreachable:
            err_msgs.emplace_back(ErrType::unreachable, block);
            fprintf(out_fd, "mov rdi, offset err_unreachable_b%zu\n", block->id);
            fprintf(out_fd, "jmp panic\n");
            break;
        case CFCInstruction::icall:
        case CFCInstruction::ijump:
            assert(0);
            exit(1);
        }
    }

    fprintf(out_fd, "\n");
}

void Generator::compile_entry() {
    compile_section(Section::TEXT);
    fprintf(out_fd, ".global _start\n");
    fprintf(out_fd, "_start:\n");
    fprintf(out_fd, "mov rbx, offset param_passing\n");
    fprintf(out_fd, "mov rdi, rsp\n");
    fprintf(out_fd, "mov rsi, offset stack_space_end\n");
    fprintf(out_fd, "call copy_stack\n");
    fprintf(out_fd, "mov [init_stack_ptr], rax\n");
    fprintf(out_fd, "jmp b%zu\n", ir->entry_block);
    fprintf(out_fd, ".type _start,STT_FUNC\n");
    fprintf(out_fd, ".size _start,$-_start\n");
}

void Generator::compile_err_msgs() {
    for (const auto &[type, block] : err_msgs) {
        switch (type) {
        case ErrType::unreachable:
            printf(R"#(err_unreachable_b%zu: .ascii "Reached unreachable code in block %zu\n\0")#"
                   "\n",
                   block->id, block->id);
            break;
        }
    }

    err_msgs.clear();
}

void Generator::compile_vars(const BasicBlock *block) {
    for (size_t idx = 0; idx < block->variables.size(); ++idx) {
        fprintf(out_fd, "# Handling var %zu\n", idx);
        const auto *var = block->variables[idx].get();
        // assert(var->info.index() != 0);
        if (var->info.index() == 0)
            continue;

        if (std::holds_alternative<size_t>(var->info)) {
            assert(var->info.index() == 2);

            // TODO: properly handle mt-statics (which are always present)
            if (var->type != Type::mt) {
                const auto *reg_str = rax_from_type(var->type);
                fprintf(out_fd, "mov rax, [s%zu]\n", std::get<size_t>(var->info));
                fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", idx, reg_str);
            }
            continue;
        }

        assert(var->info.index() != 2);
        if (var->type == Type::imm) {
            assert(var->info.index() == 1);

            const auto &info = std::get<SSAVar::ImmInfo>(var->info);
            if (info.binary_relative) {
                fprintf(out_fd, "lea rax, [binary + %ld]\n", info.val);
                fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], rax\n", idx);
            } else {
                fprintf(out_fd, "mov %s [rbp - 8 - 8 * %zu], %ld\n", ptr_from_type(var->type), idx, info.val);
            }

            continue;
        }

        assert(var->info.index() == 3);
        const auto *op = std::get<3>(var->info).get();
        assert(op != nullptr);

        std::array<const char *, 4> in_regs{};
        size_t arg_count = 0;
        for (size_t in_idx = 0; in_idx < op->in_vars.size(); ++in_idx) {
            const auto &in_var = op->in_vars[in_idx];
            if (!in_var)
                break;

            arg_count++;
            if (in_var->type == Type::mt)
                continue;

            const auto *reg_str = op_reg_map_for_type(in_var->type)[in_idx];

            fprintf(out_fd, "xor %s, %s\n", reg_str, reg_str);
            fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", reg_str, index_for_var(block, in_var));
            in_regs[in_idx] = reg_str;
        }

        switch (op->type) {
        case Instruction::store:
            assert(op->in_vars[0]->type == Type::i64);
            assert(arg_count == 2);
            fprintf(out_fd, "mov %s [%s], %s\n", ptr_from_type(op->in_vars[1]->type), in_regs[0], in_regs[1]);
            break;
        case Instruction::load:
            assert(op->in_vars[0]->type == Type::i64);
            assert(op->out_vars[0] == var);
            assert(arg_count == 2);
            fprintf(out_fd, "mov %s, %s [%s]\n", op_reg_map_for_type(var->type)[0], ptr_from_type(var->type), in_regs[0]);
            break;
        case Instruction::add:
            assert(arg_count == 2);
            fprintf(out_fd, "add rax, rbx\n");
            break;
        case Instruction::sub:
            assert(arg_count == 2);
            fprintf(out_fd, "sub rax, rbx\n");
            break;
        case Instruction::div:
            assert(arg_count == 2);
            fprintf(out_fd, "cqo\nidiv rbx\n");
            break;
        case Instruction::udiv:
            assert(arg_count == 2);
            fprintf(out_fd, "xor rdx, rdx\ndiv rbx\n");
            break;
        case Instruction::shl:
            assert(arg_count == 2);
            fprintf(out_fd, "mov cl, bl\nshl rax, cl\n");
            break;
        case Instruction::shr:
            assert(arg_count == 2);
            fprintf(out_fd, "mov cl, bl\nshr rax, cl\n");
            break;
        case Instruction::sar:
            assert(arg_count == 2);
            fprintf(out_fd, "mov cl, bl\nsar rax, cl\n");
            break;
        case Instruction::_or:
            assert(arg_count == 2);
            fprintf(out_fd, "or rax, rbx\n");
            break;
        case Instruction::_and:
            assert(arg_count == 2);
            fprintf(out_fd, "and rax, rbx\n");
            break;
        case Instruction::_not:
            assert(arg_count == 1);
            fprintf(out_fd, "not rax\n");
            break;
        case Instruction::_xor:
            assert(arg_count == 2);
            fprintf(out_fd, "xor rax, rbx\n");
            break;
        case Instruction::cast:
            assert(arg_count == 1);
            break;
        case Instruction::immediate:
            assert(false && "Immediate instruction not allowed");
            break;
        case Instruction::setup_stack:
            assert(arg_count == 0);
            fprintf(out_fd, "mov rax, [init_stack_ptr]\n");
            break;
        default:
            // TODO: ssmul_h, uumul_h, sumul_h, rem, urem, slt, sltu, sign_extend, zero_extend
            break;
        }

        if (var->type != Type::mt) {
            for (size_t out_idx = 0; out_idx < op->out_vars.size(); ++out_idx) {
                const auto &out_var = op->out_vars[out_idx];
                if (!out_var)
                    break;

                const auto *reg_str = op_reg_map_for_type(out_var->type)[out_idx];
                fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", index_for_var(block, out_var), reg_str);
            }
        }
    }
}

void Generator::compile_cf_args(const BasicBlock *block, const CfOp &cf_op) {
    const auto *target = cf_op.target();
    assert(target->inputs.size() == cf_op.target_inputs().size());
    for (size_t i = 0; i < cf_op.target_inputs().size(); ++i) {
        const auto *target_var = target->inputs[i];
        const auto &source_var = cf_op.target_inputs()[i];

        assert(target_var->type != Type::imm && target_var->info.index() > 1);

        fprintf(out_fd, "# Setting input %zu\n", i);
        if (std::holds_alternative<size_t>(target_var->info)) {
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(source_var->type), index_for_var(block, source_var));
            fprintf(out_fd, "mov [s%zu], rax\n", std::get<size_t>(target_var->info));
            continue;
        }

        // from normal var
        fprintf(out_fd, "xor rax, rax\n");
        fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(source_var->type), index_for_var(block, source_var));
        fprintf(out_fd, "mov qword ptr [rbx], rax\nadd rbx, 8\n");
    }

    // destroy stack space
    fprintf(out_fd, "# destroy stack space\n");
    fprintf(out_fd, "mov rsp, rbp\npop rbp\n");
}

void Generator::compile_ret_args(const BasicBlock *block, const CfOp &op) {
    fprintf(out_fd, "# Ret Mapping\n");
    const auto index_for_var = [block](const SSAVar *var) -> size_t {
        for (size_t idx = 0; idx < block->variables.size(); ++idx) {
            if (block->variables[idx].get() == var)
                return idx;
        }

        assert(0);
        exit(1);
    };

    assert(op.info.index() == 2);
    const auto &ret_info = std::get<CfOp::RetInfo>(op.info);
    for (const auto &[var, s_idx] : ret_info.mapping) {
        fprintf(out_fd, "xor rax, rax\n");
        fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(var->type), index_for_var(var));
        fprintf(out_fd, "mov [s%zu], rax\n", s_idx);
    }

    // destroy stack space
    fprintf(out_fd, "# destroy stack space\n");
    fprintf(out_fd, "mov rsp, rbp\npop rbp\n");
}

void Generator::compile_cjump(const BasicBlock *block, const CfOp &cf_op, const size_t cond_idx) {
    assert(cf_op.in_vars[0] != nullptr && cf_op.in_vars[1] != nullptr);
    assert(cf_op.info.index() == 1);
    // this breaks when the arg mapping is changed
    fprintf(out_fd, "# Get CJump Args\nxor rax, rax\nxor rbx, rbx\n");
    fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(cf_op.in_vars[0]->type), index_for_var(block, cf_op.in_vars[0]));
    fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", op_reg_map_for_type(cf_op.in_vars[1]->type)[1], index_for_var(block, cf_op.in_vars[1]));
    fprintf(out_fd, "cmp rax, rbx\n");

    fprintf(out_fd, "# Check CJump cond\n");
    const auto &info = std::get<CfOp::CJumpInfo>(cf_op.info);
    switch (info.type) {
    case CfOp::CJumpInfo::CJumpType::eq:
        fprintf(out_fd, "jne b%zu_cf%zu\n", block->id, cond_idx + 1);
        break;
    case CfOp::CJumpInfo::CJumpType::neq:
        fprintf(out_fd, "je b%zu_cf%zu\n", block->id, cond_idx + 1);
        break;
    case CfOp::CJumpInfo::CJumpType::lt:
        fprintf(out_fd, "jae b%zu_cf%zu\n", block->id, cond_idx + 1);
        break;
    case CfOp::CJumpInfo::CJumpType::gt:
        fprintf(out_fd, "jbe b%zu_cf%zu\n", block->id, cond_idx + 1);
        break;
    case CfOp::CJumpInfo::CJumpType::slt:
        fprintf(out_fd, "jge b%zu_cf%zu\n", block->id, cond_idx + 1);
        break;
    case CfOp::CJumpInfo::CJumpType::sgt:
        fprintf(out_fd, "jle b%zu_cf%zu\n", block->id, cond_idx + 1);
        break;
    }

    compile_cf_args(block, cf_op);
    fprintf(out_fd, "# control flow\n");
    fprintf(out_fd, "jmp b%zu\n", info.target->id);
}

void Generator::compile_syscall(const BasicBlock *block, const CfOp &cf_op) {
    const auto &info = std::get<CfOp::SyscallInfo>(cf_op.info);
    compile_continuation_args(block, info.continuation_mapping);

    for (size_t i = 0; i < call_reg.size(); ++i) {
        if (cf_op.in_vars[i] == nullptr)
            break;
        fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", call_reg[i], index_for_var(block, cf_op.in_vars[i]));
    }
    if (cf_op.in_vars[6] == nullptr) {
        fprintf(out_fd, "push 0\n");
    } else {
        fprintf(out_fd, "mov rax, [rbp - 8 - 8 * %zu]\n", index_for_var(block, cf_op.in_vars[6]));
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
}

void Generator::compile_continuation_args(const BasicBlock *block, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping) {
    for (const auto &[var, s_idx] : mapping) {
        fprintf(out_fd, "xor rax, rax\n");
        fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(var->type), index_for_var(block, var));
        fprintf(out_fd, "mov [s%zu], rax\n", s_idx);
    }
}

void Generator::compile_section(Section section) {
    switch (section) {
    case Section::DATA:
        fprintf(out_fd, ".data\n");
        break;
    case Section::BSS:
        fprintf(out_fd, ".bss\n");
        break;
    case Section::TEXT:
        fprintf(out_fd, ".text\n");
        break;
    case Section::RODATA:
        fprintf(out_fd, ".section .rodata\n");
        break;
    }
}
