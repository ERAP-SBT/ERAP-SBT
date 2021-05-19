#include "generator/x86_64/generator.h"

using namespace generator::x86_64;

// TODO: imm handling is questionable at best here

namespace
{
    std::array<const char *, 4> op_reg_mapping_64 = {"rax", "rbx", "rcx", "rdx"};

    std::array<const char *, 4> op_reg_mapping_32 = {"eax", "ebx", "ecx", "edx"};

    std::array<const char *, 4> op_reg_mapping_16 = {"ax", "bx", "cx", "dx"};

    std::array<const char *, 4> op_reg_mapping_8 = {"al", "bl", "cl", "dl"};

    std::array<const char *, 4> &op_reg_map_for_type(const Type type)
    {
        switch (type)
        {
        case Type::imm:
        case Type::i64: return op_reg_mapping_64;
        case Type::i32: return op_reg_mapping_32;
        case Type::i16: return op_reg_mapping_16;
        case Type::i8: return op_reg_mapping_8;
        case Type::f64:
        case Type::f32:
        case Type::mt: break;
        }

        assert(0);
        exit(1);
    }

    const char *rax_from_type(const Type type)
    {
        const auto *reg_str = "rax";
        switch (type)
        {
        case Type::imm:
        case Type::i64: break;
        case Type::i32: reg_str = "eax"; break;
        case Type::i16: reg_str = "ax"; break;
        case Type::i8: reg_str = "al"; break;
        case Type::f32:
        case Type::f64:
        case Type::mt: assert(0); exit(1);
        }

        return reg_str;
    }
}  // namespace

void Generator::compile()
{
    printf("global _start\n\nsection .data\n");
    compile_statics();
    printf("param_passing: resq 16\n");

    printf("section .text\n");
    compile_blocks();

    compile_entry();
}

void Generator::compile_statics()
{
    for (const auto &var : ir->statics)
    {
        std::printf("s%lu: dq 0\n", var.id);  // for now have all of the statics be 64bit
    }
}

void Generator::compile_blocks()
{
    for (auto &block : ir->basic_blocks)
        compile_block(block.get());
}

void Generator::compile_block(BasicBlock *block)
{
    const auto index_for_var = [block](const SSAVar *var) -> size_t {
        for (size_t idx = 0; idx < block->variables.size(); ++idx)
        {
            if (block->variables[idx].get() == var)
                return idx;
        }

        assert(0);
        exit(1);
    };

    for (const auto *input : block->inputs)
    {
        // don't try to compile blocks that cannot be independent for now
        if (!input->from_static)
            return;
    }

    size_t stack_size = block->variables.size() * 8;
    printf("b%zu:\npush rbp\nmov rbp, rsp\nsub rsp, %zu\n", block->id, stack_size);
    // TODO: the ir should probably at some point create an order of operations otherwise the compiler needs to create the dependency graph for variables i think
    // TODO: this relies on proper ordering of variables for now
    for (size_t idx = 0; idx < block->variables.size(); ++idx)
    {
        printf("; Handling var %zu\n", idx);
        const auto *var = block->variables[idx].get();
        assert(var->info.index() != 0);

        if (var->from_static)
        {
            assert(var->info.index() == 2);

            const auto *reg_str = rax_from_type(var->type);
            printf("mov rax, [s%zu]\n", std::get<size_t>(var->info));
            printf("mov [rbp - 8 - 8 * %zu], %s\n", idx, reg_str);
            continue;
        }

        assert(var->info.index() != 2);
        if (var->type == Type::imm)
        {
            assert(var->info.index() == 1);
            const auto *ptr_type = "qword";
            switch (var->type)
            {
            case Type::imm:
            case Type::i64: break;
            case Type::i32: ptr_type = "dword"; break;
            case Type::i16: ptr_type = "word"; break;
            case Type::i8: ptr_type = "byte"; break;
            case Type::f32:
            case Type::f64:
            case Type::mt: assert(0); exit(1);
            }

            printf("mov %s [rbp - 8 - 8 * %zu], %lld\n", ptr_type, idx, std::get<int64_t>(var->info));
            continue;
        }

        assert(var->info.index() == 3);
        const auto *op = std::get<3>(var->info).get();
        assert(op != nullptr);

        std::array<const char *, 4> in_regs{};
        for (size_t in_idx = 0; in_idx < op->in_vars.size(); ++in_idx)
        {
            const auto *in_var = op->in_vars[in_idx];
            if (!in_var)
                break;

            const auto *reg_str = op_reg_map_for_type(in_var->type)[in_idx];

            printf("xor %s, %s\n", reg_str, reg_str);
            printf("mov %s, [rbp - 8 - 8 * %zu]\n", reg_str, index_for_var(in_var));
            in_regs[in_idx] = reg_str;
        }

        switch (op->type)
        {
        case Instruction::add: printf("add rax, rbx\n"); break;
        default: assert(0); exit(1);
        }

        for (size_t out_idx = 0; out_idx < op->out_vars.size(); ++out_idx)
        {
            const auto *out_var = op->out_vars[out_idx];
            if (!out_var)
                break;

            const auto *reg_str = op_reg_map_for_type(out_var->type)[out_idx];
            printf("mov [rbp - 8 - 8 * %zu], %s\n", index_for_var(out_var), reg_str);
        }
    }

    for (size_t i = 0; i < block->control_flow_ops.size(); ++i)
    {
        const auto &cf_op = block->control_flow_ops[i];
        assert(cf_op.source == block);

        printf("b%zu_cf%zu:\n", block->id, i);
        switch (cf_op.type)
        {
        case CFCInstruction::jump:
            compile_cf_args(block, cf_op);
            printf("; control flow\n");
            printf("jmp b%zu\n", cf_op.target->id);
            break;
        case CFCInstruction::_return:
            compile_ret_args(block, cf_op);
            printf("; control flow\nret\n");
            break;
        default: assert(0); exit(1);
        }
    }

    printf("\n");
}

void Generator::compile_entry()
{
    printf("_start:\n");
    printf("mov rdi, param_passing\n");
    printf("call b%zu\n", ir->entry_block);
    printf("mov rbx, [s0]\n");
    printf("mov eax, 1\n");
    printf("int 0x80");
}

void Generator::compile_cf_args(BasicBlock *block, const CfOp &cf_op)
{
    const auto index_for_var = [block](const SSAVar *var) -> size_t {
        for (size_t idx = 0; idx < block->variables.size(); ++idx)
        {
            if (block->variables[idx].get() == var)
                return idx;
        }

        assert(0);
        exit(1);
    };

    const auto *target = cf_op.target;
    assert(target->inputs.size() == cf_op.target_inputs.size());
    for (size_t i = 0; i < cf_op.target_inputs.size(); ++i)
    {
        const auto *target_var = target->inputs[i];
        const auto *source_var = cf_op.target_inputs[i];

        assert(target_var->type != Type::imm && target_var->info.index() > 1);

        printf("; Setting input %zu\n", i);
        if (target_var->from_static)
        {
            printf("xor rax, rax\n");
            printf("mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(source_var->type), index_for_var(source_var));
            printf("mov [s%zu], rax\n", std::get<size_t>(target_var->info));
            continue;
        }

        // from normal var
        printf("xor rax, rax\n");
        printf("mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(source_var->type), index_for_var(source_var));
        printf("mov qword [rdi], rax\nadd rdi, 8\n");
    }

    // destroy stack space
    printf("; destroy stack space\n");
    printf("mov rsp, rbp\npop rbp\n");
}

void Generator::compile_ret_args(BasicBlock *block, const CfOp &op)
{
    printf("; Ret Mapping\n");
    const auto index_for_var = [block](const SSAVar *var) -> size_t {
        for (size_t idx = 0; idx < block->variables.size(); ++idx)
        {
            if (block->variables[idx].get() == var)
                return idx;
        }

        assert(0);
        exit(1);
    };

    assert(op.target == nullptr && op.info.index() == 2);
    const auto &ret_info = std::get<CfOp::RetInfo>(op.info);
    for (const auto &[var, s_idx] : ret_info.mapping)
    {
        printf("xor rax, rax\n");
        printf("mov %s, [rbp - 8 - 8 * %zu]\n", rax_from_type(var->type), index_for_var(var));
        printf("mov [s%zu], rax\n", s_idx);
    }

    // destroy stack space
    printf("; destroy stack space\n");
    printf("mov rsp, rbp\npop rbp\n");
}
