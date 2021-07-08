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
        assert(0);
        exit(1);
    }

    assert(0);
    exit(1);
}

std::array<const char *, 6> call_reg = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

const char *rax_from_type(const Type type) {
    switch (type) {
    case Type::imm:
    case Type::i64:
        return "rax";
    case Type::i32:
        return "eax";
    case Type::i16:
        return "ax";
    case Type::i8:
        return "al";
    case Type::f32:
    case Type::f64:
    case Type::mt:
        assert(0);
        exit(1);
    }

    assert(0);
    exit(1);
}

const char *ptr_from_type(const Type type) {
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
        /* TODO: extract read-write-execute information from source ELF program headers */

        /* Put the original image into a seperate section so we can set the start address */
        fprintf(out_fd, ".section .orig_binary, \"aw\"\n");
        fprintf(out_fd, ".incbin \"%s.bin\"\n", binary_filepath.c_str());
    }

    /* we expect the linker to link the original binary image (if any) at
     * exactly this address
     */
    fprintf(out_fd, ".global orig_binary_vaddr\n");
    fprintf(out_fd, "orig_binary_vaddr = %#lx\n", ir->base_addr);
    fprintf(out_fd, ".global orig_binary_size\n");
    fprintf(out_fd, "orig_binary_size = %#lx\n", ir->load_size);
    fprintf(out_fd, "binary = orig_binary_vaddr\n");

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

    compile_err_msgs();

    compile_ijump_lookup();
}

void Generator::compile_ijump_lookup() {
    compile_section(Section::RODATA);

    fprintf(out_fd, "ijump_lookup:\n");

    assert(ir->virt_bb_start_addr <= ir->virt_bb_end_addr);

    /* Incredibly space inefficient but also O(1) fast */
    for (uint64_t i = ir->virt_bb_start_addr; i < ir->virt_bb_end_addr; i += 2) {
        const auto bb = ir->bb_at_addr(i);
        if (bb != nullptr && bb->virt_start_addr == i) {
            fprintf(out_fd, ".8byte b%zu\n", bb->id);
        } else {
            fprintf(out_fd, ".8byte 0x0\n");
        }
    }

    fprintf(out_fd, "ijump_lookup_end:\n");
    fprintf(out_fd, ".type ijump_lookup,STT_OBJECT\n");
    fprintf(out_fd, ".size ijump_lookup,ijump_lookup_end-ijump_lookup\n");
}

void Generator::compile_statics() {
    compile_section(Section::DATA);

    for (const auto &var : ir->statics) {
        std::fprintf(out_fd, "s%zu: .quad 0\n", var.id); // for now have all of the statics be 64bit
    }
}

void Generator::compile_blocks() {
    compile_section(Section::TEXT);

    for (const auto &block : ir->basic_blocks) {
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
    fprintf(out_fd, "# block->virt_start_addr: %#lx\n", block->virt_start_addr);
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
        case CFCInstruction::ijump:
            compile_ijump(block, cf_op);
            break;
        case CFCInstruction::unreachable:
            err_msgs.emplace_back(ErrType::unreachable, block);
            fprintf(out_fd, "lea rdi, [rip + err_unreachable_b%zu]\n", block->id);
            fprintf(out_fd, "jmp panic\n");
            break;
        case CFCInstruction::icall:
            assert(0);
            exit(1);
        }
    }

    fprintf(out_fd, ".type b%zu,STT_FUNC\n", block->id);
    fprintf(out_fd, ".size b%zu,$-b%zu\n", block->id, block->id);

    fprintf(out_fd, "\n");
}

void Generator::compile_ijump(const BasicBlock *block, const CfOp &op) {
    assert(op.type == CFCInstruction::ijump);

    fprintf(out_fd, "# IJump Mapping\n");

    const auto &ijump_info = std::get<CfOp::IJumpInfo>(op.info);

    for (const auto &[var, s_idx] : ijump_info.mapping) {
        if (var->type == Type::mt) {
            continue;
        }

        fprintf(out_fd, "# s%zu from var v%zu\n", s_idx, var->id);
        fprintf(out_fd, "xor rax, rax\n");
        fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(var->type), index_for_var(block, var));
        fprintf(out_fd, "mov [s%zu], rax\n", s_idx);
    }

    assert(op.in_vars[0] != nullptr);
    assert(ijump_info.target == nullptr);

    fprintf(out_fd, "# Get IJump Destination\n");
    fprintf(out_fd, "xor rax, rax\n");
    fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(op.in_vars[0]->type), index_for_var(block, op.in_vars[0]));

    fprintf(out_fd, "# destroy stack space\n");
    fprintf(out_fd, "mov rsp, rbp\n");
    fprintf(out_fd, "pop rbp\n");

    err_msgs.emplace_back(ErrType::unresolved_ijump, block);

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
    fprintf(out_fd, "lea rdi, [rip + err_unresolved_ijump_b%zu]\n", block->id);
    fprintf(out_fd, "jmp panic\n");
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
    compile_section(Section::RODATA);

    for (const auto &[type, block] : err_msgs) {
        switch (type) {
        case ErrType::unreachable:
            fprintf(out_fd, "err_unreachable_b%zu: .ascii \"Reached unreachable code in block %zu\\n\\0\"\n", block->id, block->id);
            break;
        case ErrType::unresolved_ijump:
            fprintf(out_fd, "err_unresolved_ijump_b%zu: .ascii \"Reached unresolved indirect jump in block%zu\\n\\0\"\n", block->id, block->id);
            break;
        }
    }

    err_msgs.clear();
}

void Generator::compile_vars(const BasicBlock *block) {
    for (size_t idx = 0; idx < block->variables.size(); ++idx) {
        const auto *var = block->variables[idx].get();
        fprintf(out_fd, "# Handling v%zu (v%zu)\n", idx, var->id);
        if (var->info.index() == 0) {
            continue;
        }

        if (var->type == Type::mt) {
            continue;
        }

        if (std::holds_alternative<size_t>(var->info)) {
            assert(var->info.index() == 2);

            fprintf(out_fd, "mov rax, [s%zu]\n", std::get<size_t>(var->info));
            fprintf(out_fd, "mov [rbp - 8 - 8 * %zu], %s\n", idx, rax_from_type(var->type));
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
            assert(op->in_vars[0]->type == Type::i64 || op->in_vars[0]->type == Type::imm);
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

        if (target_var->type == Type::mt || source_var->type == Type::mt) {
            assert(target_var->type == source_var->type);
            continue;
        }

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

    assert(op.info.index() == 2);
    const auto &ret_info = std::get<CfOp::RetInfo>(op.info);
    for (const auto &[var, s_idx] : ret_info.mapping) {
        fprintf(out_fd, "xor rax, rax\n");
        fprintf(out_fd, "mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(var->type), index_for_var(block, var));
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

        if (cf_op.in_vars[i]->type == Type::mt)
            continue;

        fprintf(out_fd, "# syscall argument %lu\n", i);
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
        if (var->type == Type::mt) {
            continue;
        }

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
