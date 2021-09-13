#include "generator/x86_64/generator.h"

#include <iostream>

using namespace generator::x86_64;

// TODO: imm handling is questionable at best here

namespace {
std::array<const char *, 4> op_reg_mapping_64 = {"rax", "rbx", "rcx", "rdx"};

std::array<const char *, 4> op_reg_mapping_32 = {"eax", "ebx", "ecx", "edx"};

std::array<const char *, 4> op_reg_mapping_16 = {"ax", "bx", "cx", "dx"};

std::array<const char *, 4> op_reg_mapping_8 = {"al", "bl", "cl", "dl"};

std::array<const char *, 4> op_reg_mapping_fp = {"xmm0", "xmm1", "xmm2", "xmm3"};

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
    case Type::f32:
    case Type::f64:
        return op_reg_mapping_fp;
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
        return "eax";
    case Type::f64:
        return "rax";
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

const char *fp_op_size_from_type(const Type type) {
    assert(is_float(type));
    return type == Type::f32 ? "s" : "d";
}

constexpr const char *convert_name_from_type(const Type type) {
    switch (type) {
    case Type::i32:
    case Type::i64:
        return "si";
    case Type::f32:
        return "ss";
    case Type::f64:
        return "sd";
    default:
        assert(0);
        exit(0);
        break;
    }
}

constexpr bool compatible_types(const Type t1, const Type t2) { return (t1 == t2) || ((t1 == Type::imm || t2 == Type::imm) && (is_integer(t1) || is_integer(t2))); }

} // namespace

void Generator::compile() {
    assert(err_msgs.empty());

    fprintf(out_fd, ".intel_syntax noprefix\n\n");
    if (!binary_filepath.empty()) {
        /* TODO: extract read-write-execute information from source ELF program headers */

        /* Put the original image into a seperate section so we can set the start address */
        fprintf(out_fd, ".section .orig_binary, \"aw\"\n");
        fprintf(out_fd, ".incbin \"%s\"\n", binary_filepath.c_str());
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
    compile_phdr_info();

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

    if (interpreter_only) {
        compile_interpreter_only_entry();
    } else {
        compile_blocks();

        compile_entry();

        compile_err_msgs();
    }

    compile_ijump_lookup();
}

void Generator::compile_ijump_lookup() {
    compile_section(Section::RODATA);

    fprintf(out_fd, ".global ijump_lookup_base\n");
    fprintf(out_fd, ".global ijump_lookup\n");
    fprintf(out_fd, ".global ijump_lookup_end\n");

    fprintf(out_fd, "ijump_lookup_base:\n");
    fprintf(out_fd, ".8byte %zu\n", ir->virt_bb_start_addr);

    fprintf(out_fd, "ijump_lookup:\n");

    assert(ir->virt_bb_start_addr <= ir->virt_bb_end_addr);

    /* Incredibly space inefficient but also O(1) fast */
    for (uint64_t i = ir->virt_bb_start_addr; i < ir->virt_bb_end_addr; i += 2) {
        const auto bb = ir->bb_at_addr(i);
        fprintf(out_fd, "/* 0x%#.8lx: */", i);
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

    fprintf(out_fd, ".global register_file\n");
    fprintf(out_fd, "register_file:\n");

    for (const auto &var : ir->statics) {
        fprintf(out_fd, "s%zu: .quad 0\n", var.id); // for now have all of the statics be 64bit
    }
}

void Generator::compile_phdr_info() {
    std::fprintf(out_fd, "phdr_off: .quad %lu\n", ir->phdr_off);
    std::fprintf(out_fd, "phdr_num: .quad %lu\n", ir->phdr_num);
    std::fprintf(out_fd, "phdr_size: .quad %lu\n", ir->phdr_size);
    std::fprintf(out_fd, ".global phdr_off\n.global phdr_num\n.global phdr_size\n");
}

void Generator::compile_interpreter_only_entry() {
    compile_section(Section::TEXT);
    fprintf(out_fd, ".global _start\n");
    fprintf(out_fd, "_start:\n");

    // setup the RISC-V stack
    fprintf(out_fd, "mov rbx, offset param_passing\n");
    fprintf(out_fd, "mov rdi, rsp\n");
    fprintf(out_fd, "mov rsi, offset stack_space_end\n");
    fprintf(out_fd, "call copy_stack\n");

    // mov the stack pointer to the register which holds the stack pointer (refering to the calling convention)
    fprintf(out_fd, "mov [rip + register_file + 16], rax\n");

    // load the entry address of the binary and call the interpreter
    fprintf(out_fd, "mov rdi, %ld\n", ir->p_entry_addr);
    fprintf(out_fd, "call unresolved_ijump_handler\n");

    fprintf(out_fd, ".type _start,STT_FUNC\n");
    fprintf(out_fd, ".size _start,$-_start\n");
}

void Generator::compile_blocks() {
    compile_section(Section::TEXT);

    if (optimizations & OPT_MBRA) {
        reg_alloc = std::make_unique<RegAlloc>(this);
        reg_alloc->compile_blocks();
        return;
    }

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

    // align to size to 16 bytes
    const size_t stack_size = (((block->variables.size() * 8) + 15) & 0xFFFFFFFF'FFFFFFF0);
    fprintf(out_fd, "b%zu:\nsub rsp, %zu\n", block->id, stack_size);
    fprintf(out_fd, "# block->virt_start_addr: %#lx\n", block->virt_start_addr);
    compile_vars(block);

    for (size_t i = 0; i < block->control_flow_ops.size(); ++i) {
        const auto &cf_op = block->control_flow_ops[i];
        assert(cf_op.source == block);

        fprintf(out_fd, "b%zu_cf%zu:\n", block->id, i);
        switch (cf_op.type) {
        case CFCInstruction::jump:
            compile_cf_args(block, cf_op, stack_size);
            fprintf(out_fd, "# control flow\n");
            fprintf(out_fd, "jmp b%zu\n", std::get<CfOp::JumpInfo>(cf_op.info).target->id);
            break;
        case CFCInstruction::_return:
            compile_ret_args(block, cf_op, stack_size);
            fprintf(out_fd, "# control flow\nret\n");
            break;
        case CFCInstruction::cjump:
            compile_cjump(block, cf_op, i, stack_size);
            break;
        case CFCInstruction::call:
            compile_call(block, cf_op, stack_size);
            break;
        case CFCInstruction::syscall:
            compile_syscall(block, cf_op, stack_size);
            break;
        case CFCInstruction::ijump:
            compile_ijump(block, cf_op, stack_size);
            break;
        case CFCInstruction::unreachable:
            err_msgs.emplace_back(ErrType::unreachable, block);
            fprintf(out_fd, "lea rdi, [rip + err_unreachable_b%zu]\n", block->id);
            fprintf(out_fd, "jmp panic\n");
            break;
        case CFCInstruction::icall:
            compile_icall(block, cf_op, stack_size);
            break;
        }
    }

    fprintf(out_fd, ".type b%zu,STT_FUNC\n", block->id);
    fprintf(out_fd, ".size b%zu,$-b%zu\n", block->id, block->id);

    fprintf(out_fd, "\n");
}

void Generator::compile_call(const BasicBlock *block, const CfOp &op, const size_t stack_size) {
    fprintf(out_fd, "# Call Mapping\n");
    // Store statics for call
    compile_cf_args(block, op, stack_size);

    fprintf(out_fd, "# control flow\n");
    fprintf(out_fd, "call b%zu\n", std::get<CfOp::CallInfo>(op.info).target->id);

    assert(std::get<CfOp::CallInfo>(op.info).continuation_block != nullptr);
    fprintf(out_fd, "jmp b%zu\n", std::get<CfOp::CallInfo>(op.info).continuation_block->id);
}

void Generator::compile_icall(const BasicBlock *block, const CfOp &op, const size_t stack_size) {
    fprintf(out_fd, "# ICall Mapping\n");

    const auto &icall_info = std::get<CfOp::ICallInfo>(op.info);

    for (const auto &[var, s_idx] : icall_info.mapping) {
        if (var->type == Type::mt) {
            continue;
        }

        fprintf(out_fd, "# s%zu from var v%zu\n", s_idx, var->id);

        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            const auto orig_static_idx = std::get<size_t>(var->info);
            if (orig_static_idx == s_idx) {
                fprintf(out_fd, "# Skipped\n");
                continue;
            }
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [s%zu]\n", rax_from_type(var->type), orig_static_idx);
        } else {
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(var->type), index_for_var(block, var));
        }

        fprintf(out_fd, "mov [s%zu], rax\n", s_idx);
    }
    assert(op.in_vars[0] != nullptr);

    fprintf(out_fd, "# Get ICall Destination\n");
    fprintf(out_fd, "xor rax, rax\n");
    fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(op.in_vars[0]->type), index_for_var(block, op.in_vars[0]));

    err_msgs.emplace_back(ErrType::unresolved_ijump, block);

    fprintf(out_fd, "sub rax, %zu\n", ir->virt_bb_start_addr);

    fprintf(out_fd, "cmp rax, ijump_lookup_end - ijump_lookup\n");
    fprintf(out_fd, "ja 0f\n");
    fprintf(out_fd, "lea rdi, [rip + ijump_lookup]\n");
    fprintf(out_fd, "mov rdi, [rdi + 4 * rax]\n");
    fprintf(out_fd, "test rdi, rdi\n");
    fprintf(out_fd, "jne 1f\n");

    fprintf(out_fd, "0:\n");
    fprintf(out_fd, "lea rdi, [rip + err_unresolved_ijump_b%zu]\n", block->id);
    fprintf(out_fd, "jmp panic\n");

    fprintf(out_fd, "1:\n");
    fprintf(out_fd, "call rdi\n");

    fprintf(out_fd, "# destroy stack space\n");
    fprintf(out_fd, "add rsp, %zu\n", stack_size);

    assert(std::get<CfOp::ICallInfo>(op.info).continuation_block != nullptr);
    fprintf(out_fd, "jmp b%zu\n", std::get<CfOp::ICallInfo>(op.info).continuation_block->id);
}

void Generator::compile_ijump(const BasicBlock *block, const CfOp &op, const size_t stack_size) {
    assert(op.type == CFCInstruction::ijump);

    fprintf(out_fd, "# IJump Mapping\n");

    const auto &ijump_info = std::get<CfOp::IJumpInfo>(op.info);

    for (const auto &[var, s_idx] : ijump_info.mapping) {
        if (var->type == Type::mt) {
            continue;
        }

        fprintf(out_fd, "# s%zu from var v%zu\n", s_idx, var->id);

        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            const auto orig_static_idx = std::get<size_t>(var->info);
            if (orig_static_idx == s_idx) {
                fprintf(out_fd, "# Skipped\n");
                continue;
            }
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [s%zu]\n", rax_from_type(var->type), orig_static_idx);
        } else {
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(var->type), index_for_var(block, var));
        }

        fprintf(out_fd, "mov [s%zu], rax\n", s_idx);
    }

    assert(op.in_vars[0] != nullptr);
    assert(ijump_info.targets.empty());
    assert((op.in_vars[0]->type == Type::i64) || (op.in_vars[0]->type == Type::imm)); // TODO: only one should be used

    fprintf(out_fd, "# Get IJump Destination\n");
    fprintf(out_fd, "xor rax, rax\n");
    fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(op.in_vars[0]->type), index_for_var(block, op.in_vars[0]));

    fprintf(out_fd, "# destroy stack space\n");
    fprintf(out_fd, "add rsp, %zu\n", stack_size);

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

    /* Slow-path: unresolved IJump, call interpreter */
    fprintf(out_fd, "add rax, %zu\n", ir->virt_bb_start_addr);
    fprintf(out_fd, "mov rdi, rax\n");
    fprintf(out_fd, "lea rsi, [rip + err_unresolved_ijump_b%zu]\n", block->id);
    fprintf(out_fd, "jmp unresolved_ijump\n");
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

        if (std::holds_alternative<size_t>(var->info)) {
            if (var->type == Type::mt) {
                continue;
            }

            if (optimizations & OPT_UNUSED_STATIC) {
                auto has_var_ref = false;
                for (size_t j = idx + 1; j < block->variables.size(); ++j) {
                    const auto *var2 = block->variables[j].get();
                    if (!std::holds_alternative<std::unique_ptr<Operation>>(var2->info)) {
                        continue;
                    }

                    const auto *op = std::get<std::unique_ptr<Operation>>(var2->info).get();
                    for (const auto &in_var : op->in_vars) {
                        if (in_var && in_var == var) {
                            has_var_ref = true;
                            break;
                        }
                    }
                    if (has_var_ref) {
                        break;
                    }
                }

                if (!has_var_ref) {
                    fprintf(out_fd, "# Skipped\n");
                    continue;
                }
            }

            const auto *reg_str = rax_from_type(var->type);
            fprintf(out_fd, "mov rax, [s%zu]\n", std::get<size_t>(var->info));
            fprintf(out_fd, "mov [rsp + 8 * %zu], %s\n", idx, reg_str);
            continue;
        }

        assert(var->info.index() != 2);
        if (var->info.index() == 1) {
            const auto &info = std::get<SSAVar::ImmInfo>(var->info);
            if (info.binary_relative) {
                fprintf(out_fd, "lea rax, [binary + %ld]\n", info.val);
                fprintf(out_fd, "mov [rsp + 8 * %zu], rax\n", idx);
            } else {
                // use other loading if the immediate is to big
                if (info.val > INT32_MAX || info.val < INT32_MIN) {
                    fprintf(out_fd, "mov rax, %ld\n", info.val);
                    fprintf(out_fd, "mov QWORD PTR [rsp + 8 * %zu], rax\n", idx);
                } else {
                    fprintf(out_fd, "mov %s [rsp + 8 * %zu], %ld\n", ptr_from_type(var->type), idx, info.val);
                }
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

            // zero the full register so stuff doesn't go broke e.g. in zero-extend, cast
            if (is_float(in_var->type)) {
                fprintf(out_fd, "pxor %s, %s\n", reg_str, reg_str);
                fprintf(out_fd, "mov%s %s, [rsp + 8 * %zu]\n", (in_var->type == Type::f32 ? "d" : "q"), reg_str, index_for_var(block, in_var));
            } else {
                const auto *full_reg_str = op_reg_map_for_type(Type::i64)[in_idx];
                fprintf(out_fd, "xor %s, %s\n", full_reg_str, full_reg_str);
                fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", reg_str, index_for_var(block, in_var));
            }
            in_regs[in_idx] = reg_str;
        }

        auto set_if_op = [this, var, op, in_regs, arg_count](const char *cc_i_1, const char *cc_i_2, const char *cc_fp_1, const char *cc_fp_2, bool allow_fp = true) {
            assert(arg_count == 4);
            SSAVar *const in1 = op->in_vars[0];
            SSAVar *const in2 = op->in_vars[1];
            SSAVar *const in3 = op->in_vars[2];
            SSAVar *const in4 = op->in_vars[3];
            const char *cc_1, *cc_2;
            if (is_float(in1->type) || is_float(in2->type)) {
                assert(allow_fp);
                assert(in1->type == in2->type);
                assert(!is_float(var->type));
                assert(compatible_types(var->type, in3->type) && compatible_types(var->type, in4->type));
                fprintf(out_fd, "comis%s %s, %s\n", fp_op_size_from_type(in1->type), in_regs[0], in_regs[1]);
                cc_1 = cc_fp_1;
                cc_2 = cc_fp_2;
            } else {
                const Type cmp_type = (in1->type == Type::imm ? (in2->type == Type::imm ? Type::i64 : in2->type) : in1->type);
                assert(compatible_types(var->type, in3->type) && compatible_types(var->type, in4->type));
                fprintf(out_fd, "cmp %s, %s\n", op_reg_map_for_type(cmp_type)[0], op_reg_map_for_type(cmp_type)[1]);
                cc_1 = cc_i_1;
                cc_2 = cc_i_2;
            }
            fprintf(out_fd, "cmov%s %s, %s\n", cc_1, rax_from_type(var->type), op_reg_map_for_type(var->type)[2]);
            fprintf(out_fd, "cmov%s %s, %s\n", cc_2, rax_from_type(var->type), op_reg_map_for_type(var->type)[3]);
        };

        switch (op->type) {
        case Instruction::store:
            assert(op->in_vars[0]->type == Type::i64 || op->in_vars[0]->type == Type::imm);
            assert(arg_count == 3);
            if (is_float(op->in_vars[1]->type)) {
                fprintf(out_fd, "mov%s [%s], %s\n", (op->in_vars[1]->type == Type::f32 ? "d" : "q"), in_regs[0], in_regs[1]);
            } else {
                fprintf(out_fd, "mov %s [%s], %s\n", ptr_from_type(op->in_vars[1]->type), in_regs[0], in_regs[1]);
            }
            break;
        case Instruction::load:
            assert(op->in_vars[0]->type == Type::i64 || op->in_vars[0]->type == Type::imm);
            assert(op->out_vars[0] == var);
            assert(arg_count == 2);
            if (is_float(var->type)) {
                fprintf(out_fd, "mov%s xmm0, [%s]\n", (op->in_vars[1]->type == Type::f32 ? "d" : "q"), in_regs[0]);
            } else {
                fprintf(out_fd, "mov %s, %s [%s]\n", op_reg_map_for_type(var->type)[0], ptr_from_type(var->type), in_regs[0]);
            }
            break;
        case Instruction::add:
            assert(arg_count == 2);
            if (is_float(var->type)) {
                assert(var->type == op->in_vars[0]->type && op->in_vars[0]->type == op->in_vars[1]->type);
                fprintf(out_fd, "adds%s xmm0, xmm1\n", fp_op_size_from_type(op->in_vars[0]->type));
            } else {
                fprintf(out_fd, "add rax, rbx\n");
            }
            break;
        case Instruction::sub:
            assert(arg_count == 2);
            if (is_float(var->type)) {
                assert(var->type == op->in_vars[0]->type && op->in_vars[0]->type == op->in_vars[1]->type);
                fprintf(out_fd, "subs%s xmm0, xmm1\n", fp_op_size_from_type(op->in_vars[0]->type));
            } else {
                fprintf(out_fd, "sub rax, rbx\n");
            }
            break;
        case Instruction::mul_l:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "imul rax, rbx\n");
            break;
        case Instruction::ssmul_h:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "imul rbx\nmov rax, rdx\n");
            break;
        case Instruction::uumul_h:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "mul rbx\nmov rax, rdx\n");
            break;
        case Instruction::div:
            assert(arg_count == 2 || arg_count == 3);
            assert(!is_float(var->type));
            if (var->type == Type::i32) {
                fprintf(out_fd, "cdq\nidiv ebx\n");
            } else {
                fprintf(out_fd, "cqo\nidiv rbx\n");
            }
            fprintf(out_fd, "mov rbx, rdx\n"); // second output is remainder and needs to be in rbx atm
            break;
        case Instruction::udiv:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "xor rdx, rdx\ndiv rbx\n");
            fprintf(out_fd, "mov rbx, rdx\n"); // second output is remainder and needs to be in rbx atm
            break;
        case Instruction::shl:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "mov cl, bl\nshl %s, cl\n", rax_from_type(op->in_vars[0]->type));
            break;
        case Instruction::shr:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "mov cl, bl\nshr %s, cl\n", rax_from_type(op->in_vars[0]->type));
            break;
        case Instruction::sar:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            // make sure that it uses the bit-width of the input operand for shifting
            // so that the sign-bit is properly recognized
            // TODO: find out if that is a problem elsewhere
            fprintf(out_fd, "mov cl, bl\nsar %s, cl\n", rax_from_type(op->in_vars[0]->type));
            break;
        case Instruction::_or:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "or rax, rbx\n");
            break;
        case Instruction::_and:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "and rax, rbx\n");
            break;
        case Instruction::_not:
            assert(arg_count == 1);
            assert(!is_float(var->type));
            fprintf(out_fd, "not rax\n");
            break;
        case Instruction::_xor:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "xor rax, rbx\n");
            break;
        case Instruction::cast:
            assert(arg_count == 1);
            if (is_float(var->type)) {
                if (op->in_vars[0]->type == Type::f64 && var->type == Type::f32) {
                    // nothing to be done
                } else if (is_integer(op->in_vars[0]->type)) {
                    fprintf(out_fd, "mov%s xmm0, %s\n", (var->type == Type::f32 ? "d" : "q"), in_regs[0]);
                } else {
                    assert(0);
                }
            } else if (is_integer(var->type) && is_float(op->in_vars[0]->type)) {
                fprintf(out_fd, "mov%s %s, xmm0\n", (var->type == Type::i32 ? "d" : "q"), rax_from_type(var->type));
            }
            break;
        case Instruction::setup_stack:
            assert(arg_count == 0);
            fprintf(out_fd, "mov rax, [init_stack_ptr]\n");
            break;
        case Instruction::zero_extend:
            assert(arg_count == 1);
            // nothing to be done
            break;
        case Instruction::sign_extend:
            assert(arg_count == 1);
            assert(!is_float(var->type));
            if (op->in_vars[0]->type != Type::i64) {
                fprintf(out_fd, "movsx rax, %s\n", in_regs[0]);
            }
            break;
        case Instruction::slt:
            set_if_op("l", "ge", "b", "ae");
            break;
        case Instruction::sltu:
            set_if_op("b", "ae", "", "", false);
            break;
        case Instruction::sumul_h: /* TODO: implement */
            assert(0);
            break;
        case Instruction::umax:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "cmp %s, %s\n", in_regs[0], in_regs[1]);
            fprintf(out_fd, "cmova %s, %s\n", in_regs[0], in_regs[1]);
            fprintf(out_fd, "mov %s, %s\n", rax_from_type(op->in_vars[0]->type), in_regs[0]);
            break;
        case Instruction::umin:
            assert(arg_count == 2);
            assert(!is_float(var->type));
            fprintf(out_fd, "cmp %s, %s\n", in_regs[0], in_regs[1]);
            fprintf(out_fd, "cmovb %s, %s\n", in_regs[0], in_regs[1]);
            fprintf(out_fd, "mov %s, %s\n", rax_from_type(op->in_vars[0]->type), in_regs[0]);
            break;
        case Instruction::max:
            assert(arg_count == 2);
            if (is_float(var->type)) {
                fprintf(out_fd, "maxs%s xmm0, xmm1\n", fp_op_size_from_type(var->type));
            } else {
                fprintf(out_fd, "cmp %s, %s\n", in_regs[0], in_regs[1]);
                fprintf(out_fd, "cmovg %s, %s\n", in_regs[0], in_regs[1]);
                fprintf(out_fd, "mov %s, %s\n", rax_from_type(op->in_vars[0]->type), in_regs[0]);
            }
            break;
        case Instruction::min:
            assert(arg_count == 2);
            if (is_float(var->type)) {
                fprintf(out_fd, "mins%s xmm0, xmm1\n", fp_op_size_from_type(var->type));
            } else {
                fprintf(out_fd, "cmp %s, %s\n", in_regs[0], in_regs[1]);
                fprintf(out_fd, "cmovl %s, %s\n", in_regs[0], in_regs[1]);
                fprintf(out_fd, "mov %s, %s\n", rax_from_type(op->in_vars[0]->type), in_regs[0]);
            }
            break;
        case Instruction::sle:
            set_if_op("le", "g", "be", "a");
            break;
        case Instruction::seq:
            set_if_op("e", "ne", "e", "ne");
            break;
        case Instruction::fmul:
            assert(arg_count == 2);
            assert(is_float(var->type));
            assert(var->type == op->in_vars[0]->type && var->type == op->in_vars[1]->type);
            fprintf(out_fd, "muls%s xmm0, xmm1\n", fp_op_size_from_type(var->type));
            break;
        case Instruction::fdiv:
            assert(arg_count == 2);
            assert(is_float(var->type));
            assert(var->type == op->in_vars[0]->type && var->type == op->in_vars[1]->type);
            fprintf(out_fd, "divs%s xmm0, xmm1\n", fp_op_size_from_type(var->type));
            break;
        case Instruction::fsqrt:
            assert(arg_count == 1);
            assert(is_float(var->type));
            assert(var->type == op->in_vars[0]->type);
            fprintf(out_fd, "sqrts%s xmm0, xmm0\n", fp_op_size_from_type(var->type));
            break;
        case Instruction::fmadd:
            assert(arg_count == 3);
            assert(is_float(var->type));
            assert(var->type == op->in_vars[0]->type && var->type == op->in_vars[1]->type && var->type == op->in_vars[2]->type);
            fprintf(out_fd, "muls%s xmm0, xmm1\n", fp_op_size_from_type(var->type));
            fprintf(out_fd, "adds%s xmm0, xmm2\n", fp_op_size_from_type(var->type));
            break;
        case Instruction::fmsub:
            assert(arg_count == 3);
            assert(is_float(var->type));
            assert(var->type == op->in_vars[0]->type && var->type == op->in_vars[1]->type && var->type == op->in_vars[2]->type);
            fprintf(out_fd, "muls%s xmm0, xmm1\n", fp_op_size_from_type(var->type));
            fprintf(out_fd, "subs%s xmm0, xmm2\n", fp_op_size_from_type(var->type));
            break;
        case Instruction::fnmadd: {
            assert(arg_count == 3);
            assert(is_float(var->type));
            assert(var->type == op->in_vars[0]->type && var->type == op->in_vars[1]->type && var->type == op->in_vars[2]->type);
            const bool is_single_precision = var->type == Type::f32;
            fprintf(out_fd, "muls%s xmm0, xmm1\n", fp_op_size_from_type(var->type));
            // toggle the sign of the result (negate) of the product by using a mask and xor
            fprintf(out_fd, "mov rax, %s\n", is_single_precision ? "0x80000000" : "0x8000000000000000");
            fprintf(out_fd, "mov%s xmm3, rax\n", is_single_precision ? "d" : "q");
            fprintf(out_fd, "pxor xmm0, xmm3\n");
            fprintf(out_fd, "adds%s xmm0, xmm2\n", fp_op_size_from_type(var->type));
            break;
        }
        case Instruction::fnmsub: {
            assert(arg_count == 3);
            assert(is_float(var->type));
            assert(var->type == op->in_vars[0]->type && var->type == op->in_vars[1]->type && var->type == op->in_vars[2]->type);
            const bool is_single_precision = var->type == Type::f32;
            fprintf(out_fd, "muls%s xmm0, xmm1\n", fp_op_size_from_type(var->type));
            // toggle the sign of the result (negate) of the product by using a mask and xor
            fprintf(out_fd, "mov rax, %s\n", is_single_precision ? "0x80000000" : "0x8000000000000000");
            fprintf(out_fd, "mov%s xmm3, rax\n", is_single_precision ? "d" : "q");
            fprintf(out_fd, "pxor xmm0, xmm3\n");
            fprintf(out_fd, "subs%s xmm0, xmm2\n", fp_op_size_from_type(var->type));
            break;
        }
        case Instruction::convert:
            assert(arg_count == 1);
            assert(is_float(var->type) || is_float(op->in_vars[0]->type));
            if (is_integer(var->type)) {
                compile_rounding_mode(var);
            }
            fprintf(out_fd, "cvt%s2%s %s, %s\n", convert_name_from_type(op->in_vars[0]->type), convert_name_from_type(var->type), (is_float(var->type) ? "xmm0" : rax_from_type(var->type)),
                    (is_float(op->in_vars[0]->type) ? "xmm0" : rax_from_type(op->in_vars[0]->type)));
            break;
        case Instruction::uconvert: {
            assert(arg_count == 1);
            const Type in_var_type = op->in_vars[0]->type;
            assert(is_float(var->type) || is_float(in_var_type));
            if (is_float(in_var_type)) {
                compile_rounding_mode(var);
                const bool is_single_precision = in_var_type == Type::f32;
                // spread the sign bit of the floating point number to the length of the (result) integer.
                // Negate this value and with an "and" operation set so the result to zero if the floating point value is negative
                const char *help_reg_name = (is_single_precision ? "ebx" : "rbx");
                fprintf(out_fd, "mov%s %s, xmm0\n", (is_single_precision ? "d" : "q"), help_reg_name);
                fprintf(out_fd, "sar %s, %u\n", help_reg_name, (is_single_precision ? 31 : 63));
                fprintf(out_fd, "not %s\n", help_reg_name);
                if (is_single_precision && var->type == Type::i64) {
                    fprintf(out_fd, "movsxd rbx, ebx\n");
                }
                fprintf(out_fd, "cvt%s2%s %s, xmm0\n", convert_name_from_type(in_var_type), convert_name_from_type(var->type), rax_from_type(var->type));
                fprintf(out_fd, "and rax, rbx\n");
            } else {
                if (in_var_type == Type::i32) {
                    // "zero extend" and then convert: use 64bit register
                    fprintf(out_fd, "mov eax, eax\n");
                    fprintf(out_fd, "cvt%s2%s xmm0, rax\n", convert_name_from_type(Type::i32), convert_name_from_type(var->type));
                } else if (in_var_type == Type::i64) {
                    // method taken from gcc compiler
                    fprintf(out_fd, "mov rbx, rax\n");
                    fprintf(out_fd, "shr rax\n");
                    fprintf(out_fd, "and rbx, 1\n");
                    fprintf(out_fd, "or rax, rbx\n");
                    fprintf(out_fd, "cvt%s2%s xmm0, rax\n", convert_name_from_type(Type::i64), convert_name_from_type(var->type));
                    fprintf(out_fd, "adds%s xmm0, xmm0\n", fp_op_size_from_type(var->type));
                } else {
                    assert(0);
                }
            }
            break;
        }
        }

        if (var->type != Type::mt) {
            if (is_float(var->type)) {
                fprintf(out_fd, "mov%s [rsp + 8 * %zu], xmm0\n", (var->type == Type::f32 ? "d" : "q"), index_for_var(block, var));
            } else {
                for (size_t out_idx = 0; out_idx < op->out_vars.size(); ++out_idx) {
                    const auto &out_var = op->out_vars[out_idx];
                    if (!out_var)
                        continue;

                    const auto *reg_str = op_reg_map_for_type(out_var->type)[out_idx];
                    fprintf(out_fd, "mov [rsp + 8 * %zu], %s\n", index_for_var(block, out_var), reg_str);
                }
            }
        }
    }
}

void Generator::compile_rounding_mode(const SSAVar *var) {
    assert(std::holds_alternative<std::unique_ptr<Operation>>(var->info));
    auto &rounding_mode_variant = std::get<std::unique_ptr<Operation>>(var->info).get()->rounding_info;
    if (std::holds_alternative<SSAVar *>(rounding_mode_variant)) {
        // TODO: Handle dynamic rounding
        assert(0);
    } else if (std::holds_alternative<RoundingMode>(rounding_mode_variant)) {
        uint32_t x86_64_rounding_mode;
        switch (std::get<RoundingMode>(rounding_mode_variant)) {
        case RoundingMode::NEAREST:
            x86_64_rounding_mode = 0x0000;
            break;
        case RoundingMode::DOWN:
            x86_64_rounding_mode = 0x2000;
            break;
        case RoundingMode::UP:
            x86_64_rounding_mode = 0x4000;
            break;
        case RoundingMode::ZERO:
            x86_64_rounding_mode = 0x6000;
            break;
        default:
            assert(0);
            break;
        }
        // clear rounding mode and set correctly
        fprintf(out_fd, "sub rsp, 4\n");
        fprintf(out_fd, "STMXCSR [rsp]\n");
        fprintf(out_fd, "mov edi, [rsp]\n");
        fprintf(out_fd, "and edi, 0xFFFF1FFF\n");
        if (x86_64_rounding_mode != 0) {
            fprintf(out_fd, "or edi, %u\n", x86_64_rounding_mode);
        }
        fprintf(out_fd, "mov [rsp], edi\n");
        fprintf(out_fd, "add rsp, 4\n");
    } else {
        assert(0);
    }
}

void Generator::compile_cf_args(const BasicBlock *block, const CfOp &cf_op, const size_t stack_size) {
    const auto *target = cf_op.target();
    const auto &target_inputs = cf_op.target_inputs();
    if (target->inputs.size() != target_inputs.size()) {
        std::cout << "target->inputs.size() = " << target->inputs.size() << "\n";
        std::cout << "cf_op.target_inputs().size() = " << target_inputs.size() << "\n";
    }
    assert(target->inputs.size() == target_inputs.size());
    for (size_t i = 0; i < target_inputs.size(); ++i) {
        const auto *target_var = target->inputs[i];
        const auto *source_var = target_inputs[i];

        assert(target_var->type != Type::imm && target_var->info.index() > 1);

        if (target_var->type == Type::mt || source_var->type == Type::mt) {
            assert(target_var->type == source_var->type);
            continue;
        }

        fprintf(out_fd, "# Setting input %zu\n", i);

        const auto target_is_static = std::holds_alternative<size_t>(target_var->info);
        if (std::holds_alternative<size_t>(source_var->info)) {
            if (optimizations & OPT_UNUSED_STATIC) {
                if (target_is_static && std::get<size_t>(source_var->info) == std::get<size_t>(target_var->info)) {
                    // TODO: see this as a different optimization?
                    fprintf(out_fd, "# Skipped\n");
                    continue;
                } else {
                    // when using the unused static optimization, the static load might have been optimized out
                    // so we need to get the static directly
                    fprintf(out_fd, "xor rax, rax\n");
                    fprintf(out_fd, "mov %s, [s%zu]\n", rax_from_type(source_var->type), std::get<size_t>(source_var->info));
                }
            } else {
                fprintf(out_fd, "xor rax, rax\n");
                fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(source_var->type), index_for_var(block, source_var));
            }
        } else {
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(source_var->type), index_for_var(block, source_var));
        }

        if (target_is_static) {
            fprintf(out_fd, "mov [s%zu], rax\n", std::get<size_t>(target_var->info));
        } else {
            fprintf(out_fd, "mov qword ptr [rbx], rax\nadd rbx, 8\n");
        }
    }

    // destroy stack space
    fprintf(out_fd, "# destroy stack space\n");
    fprintf(out_fd, "add rsp, %zu\n", stack_size);
}

void Generator::compile_ret_args(const BasicBlock *block, const CfOp &op, const size_t stack_size) {
    fprintf(out_fd, "# Ret Mapping\n");

    assert(op.info.index() == 2);
    const auto &ret_info = std::get<CfOp::RetInfo>(op.info);
    for (const auto &[var, s_idx] : ret_info.mapping) {
        if (var->type == Type::mt) {
            continue;
        }

        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            if (std::get<size_t>(var->info) == s_idx) {
                fprintf(out_fd, "# Skipped\n");
                continue;
            }
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [s%zu]\n", rax_from_type(var->type), std::get<size_t>(var->info));
        } else {
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(var->type), index_for_var(block, var));
        }

        fprintf(out_fd, "mov [s%zu], rax\n", s_idx);
    }

    // destroy stack space
    fprintf(out_fd, "# destroy stack space\n");
    fprintf(out_fd, "add rsp, %zu\n", stack_size);
}

void Generator::compile_cjump(const BasicBlock *block, const CfOp &cf_op, const size_t cond_idx, const size_t stack_size) {
    assert(cf_op.in_vars[0] != nullptr && cf_op.in_vars[1] != nullptr);
    assert(cf_op.info.index() == 1);
    // this breaks when the arg mapping is changed
    fprintf(out_fd, "# Get CJump Args\nxor rax, rax\nxor rbx, rbx\n");
    if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(cf_op.in_vars[0]->info)) {
        // load might be optimized out so get the value directly
        fprintf(out_fd, "mov %s, [s%zu]\n", rax_from_type(cf_op.in_vars[0]->type), std::get<size_t>(cf_op.in_vars[0]->info));
    } else {
        fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(cf_op.in_vars[0]->type), index_for_var(block, cf_op.in_vars[0]));
    }
    if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(cf_op.in_vars[1]->info)) {
        // load might be optimized out so get the value directly
        fprintf(out_fd, "mov %s, [s%zu]\n", op_reg_map_for_type(cf_op.in_vars[1]->type)[1], std::get<size_t>(cf_op.in_vars[1]->info));
    } else {
        fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", op_reg_map_for_type(cf_op.in_vars[1]->type)[1], index_for_var(block, cf_op.in_vars[1]));
    }
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

    compile_cf_args(block, cf_op, stack_size);
    fprintf(out_fd, "# control flow\n");
    fprintf(out_fd, "jmp b%zu\n", info.target->id);
}

void Generator::compile_syscall(const BasicBlock *block, const CfOp &cf_op, const size_t stack_size) {
    const auto &info = std::get<CfOp::SyscallInfo>(cf_op.info);
    compile_continuation_args(block, info.continuation_mapping);

    for (size_t i = 0; i < call_reg.size(); ++i) {
        const auto &var = cf_op.in_vars[i];
        if (!var)
            break;

        if (var->type == Type::mt)
            continue;

        fprintf(out_fd, "# syscall argument %lu\n", i);
        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            fprintf(out_fd, "mov %s, [s%zu]\n", call_reg[i], std::get<size_t>(var->info));
        } else {
            fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", call_reg[i], index_for_var(block, var));
        }
    }
    if (cf_op.in_vars[6] == nullptr) {
        // since the function can theoretically change the value on the stack we do want to add some space here
        fprintf(out_fd, "sub rsp, 16\n");
    } else {
        fprintf(out_fd, "mov rax, [rsp + 8 * %zu]\n", index_for_var(block, cf_op.in_vars[6]));
        fprintf(out_fd, "sub rsp, 8\n");
        fprintf(out_fd, "push rax\n");
    }

    fprintf(out_fd, "call syscall_impl\nadd rsp, 16\n");
    if (info.static_mapping.size() > 0) {
        fprintf(out_fd, "mov [s%zu], rax\n", info.static_mapping.at(0));
    }
    fprintf(out_fd, "# destroy stack space\n");
    fprintf(out_fd, "add rsp, %zu\n", stack_size);
    fprintf(out_fd, "jmp b%zu\n", info.continuation_block->id);
}

void Generator::compile_continuation_args(const BasicBlock *block, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping) {
    for (const auto &[var, s_idx] : mapping) {
        if (var->type == Type::mt) {
            continue;
        }

        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            const auto orig_static_idx = std::get<size_t>(var->info);
            if (orig_static_idx == s_idx) {
                fprintf(out_fd, "# Skipped\n");
                continue;
            }
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [s%zu]\n", rax_from_type(var->type), orig_static_idx);
        } else {
            fprintf(out_fd, "xor rax, rax\n");
            fprintf(out_fd, "mov %s, [rsp + 8 * %zu]\n", rax_from_type(var->type), index_for_var(block, var));
        }

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
