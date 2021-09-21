#include "generator/x86_64/generator.h"

#include "generator/x86_64/assembler.h"

using namespace generator::x86_64;

// TODO: imm handling is questionable at best here

namespace {
std::array<uint64_t, 4> op_reg_mapping = {FE_AX, FE_BX, FE_CX, FE_DX};

std::array<uint64_t, 6> call_reg = {FE_DI, FE_SI, FE_DX, FE_CX, FE_R8, FE_R9};

size_t index_for_var(const BasicBlock *block, const SSAVar *var) {
    for (size_t idx = 0; idx < block->variables.size(); ++idx) {
        if (block->variables[idx].get() == var)
            return idx;
    }

    assert(0);
    exit(1);
}
} // namespace

Generator::Generator(IR *ir, std::string binary_filepath, FILE *out_fd, std::string binary_out)
    : ir(ir), binary_filepath(std::move(binary_filepath)), binary_out(std::move(binary_out)), out_fd(out_fd) {
    auto file = std::ifstream{this->binary_filepath, std::ios::binary};
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    auto vec = std::vector<uint8_t>{};
    vec.resize(file_size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(vec.data()), vec.size());
    as = Assembler{ir, std::move(vec)};
}

void Generator::compile() {
    assert(err_msgs.empty());

    compile_blocks();

    as.finish(binary_out, this);
}

void Generator::compile_blocks() {
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

    // +8 bc we push rbp
    const size_t stack_size = (((block->variables.size() * 8) + 15) & 0xFFFFFFFF'FFFFFFF0) + 8;
    as.start_new_bb(block->id);
    fe_enc64(as.cur_instr_ptr(), FE_PUSHr, FE_BP);               // push rbp
    fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_BP, FE_SP);      // mov rbp, rsp
    fe_enc64(as.cur_instr_ptr(), FE_SUB64ri, FE_SP, stack_size); // sub rsp, stack_size
    compile_vars(block);

    needs_short_jmp_resolve = false;
    for (size_t i = 0; i < block->control_flow_ops.size(); ++i) {
        const auto &cf_op = block->control_flow_ops[i];
        assert(cf_op.source == block);

        if (needs_short_jmp_resolve) {
            as.resolve_short_jmp();
            needs_short_jmp_resolve = false;
        }
        switch (cf_op.type) {
        case CFCInstruction::jump:
            compile_cf_args(block, cf_op);
            as.add_jmp_to_bb(std::get<CfOp::JumpInfo>(cf_op.info).target->id);
            break;
        case CFCInstruction::_return:
            compile_ret_args(block, cf_op);
            fe_enc64(as.cur_instr_ptr(), FE_RET);
            break;
        case CFCInstruction::cjump:
            compile_cjump(block, cf_op, i);
            break;
        case CFCInstruction::call:
            assert(0);
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
            assert(0);
            err_msgs.emplace_back(ErrType::unreachable, block);
            as.add_panic(err_msgs.size() - 1);
            // fprintf(out_fd, "lea rdi, [rip + err_unreachable_b%zu]\n", block->id);
            // fprintf(out_fd, "jmp panic\n");
            break;
        case CFCInstruction::icall:
            assert(0);
            exit(1);
        }
    }

    // TODO: as.end_bb()?
}

void Generator::compile_ijump(const BasicBlock *block, const CfOp &op) {
    assert(op.type == CFCInstruction::ijump);

    const auto &ijump_info = std::get<CfOp::IJumpInfo>(op.info);

    for (const auto &[var, s_idx] : ijump_info.mapping) {
        if (var->type == Type::mt) {
            continue;
        }

        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            const auto orig_static_idx = std::get<size_t>(var->info);
            if (orig_static_idx == s_idx) {
                continue;
            }
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
            as.load_static_in_reg(orig_static_idx, FE_AX, var->type);
        } else {
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
            fe_enc64(as.cur_instr_ptr(), opcode(var->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), FE_AX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, var))));
        }

        as.save_static_from_reg(s_idx, FE_AX, Type::i64);
    }

    assert(op.in_vars[0] != nullptr);
    assert(ijump_info.target == nullptr);

    // get ijump destination
    fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
    fe_enc64(as.cur_instr_ptr(), opcode(op.in_vars[0]->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), FE_AX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, op.in_vars[0]))));

    fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_SP, FE_BP);
    fe_enc64(as.cur_instr_ptr(), FE_POPr, FE_BP);

    err_msgs.emplace_back(ErrType::unresolved_ijump, block);

    /* we trust the lifter that the ijump destination is already aligned */

    /* turn absolute address into relative offset from start of first basicblock */
    fe_enc64(as.cur_instr_ptr(), FE_SUB64ri, FE_AX, ir->virt_bb_start_addr);

    fe_enc64(as.cur_instr_ptr(), FE_CMP64ri, FE_AX, as.ijump_table_size); // TODO: this bounds check is wrong
    auto *cur1 = *as.cur_instr_ptr();
    fe_enc64(&as.instr_ptr, FE_JA | FE_JMPL, (intptr_t)cur1);
    fe_enc64(as.cur_instr_ptr(), FE_MOV64ri, FE_DI, as.ijump_table_addr);
    fe_enc64(as.cur_instr_ptr(), FE_MOV64rm, FE_DI, FE_MEM(FE_DI, 4, FE_AX, 0));
    fe_enc64(as.cur_instr_ptr(), FE_TEST64rr, FE_DI, FE_DI);
    auto *cur2 = *as.cur_instr_ptr();
    fe_enc64(&as.instr_ptr, FE_JZ | FE_JMPL, (intptr_t)cur1);
    fe_enc64(as.cur_instr_ptr(), FE_JMPr, FE_DI);

    fe_enc64(&cur1, FE_JA | FE_JMPL, (intptr_t)as.instr_ptr);
    fe_enc64(&cur2, FE_JZ | FE_JMPL, (intptr_t)as.instr_ptr);
    as.add_panic(err_msgs.size() - 1);
}

void Generator::compile_vars(const BasicBlock *block) {
    for (size_t idx = 0; idx < block->variables.size(); ++idx) {
        const auto *var = block->variables[idx].get();
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
                    continue;
                }
            }

            as.load_static_in_reg(std::get<size_t>(var->info), FE_AX, var->type);
            fe_enc64(as.cur_instr_ptr(), opcode(var->type, FE_MOV64mr, FE_MOV32mr, FE_MOV16mr, FE_MOV8mr), FE_MEM(FE_BP, 0, 0, -(8 + 8 * idx)), FE_AX); // mov [rbp - 8 - 8 * idx], rax
            continue;
        }

        assert(var->info.index() != 2);
        if (var->type == Type::imm) {
            assert(var->info.index() == 1);

            const auto &info = std::get<SSAVar::ImmInfo>(var->info);
            if (info.binary_relative) {
                as.load_binary_rel_val(FE_AX, info.val);
                fe_enc64(as.cur_instr_ptr(), FE_MOV64mr, FE_MEM(FE_BP, 0, 0, -(8 + 8 * idx)), FE_AX);
            } else {
                // TODO: this is 32 bit sign extended
                fe_enc64(as.cur_instr_ptr(), opcode(var->type, FE_MOV64mi, FE_MOV32mi, FE_MOV16mi, FE_MOV8mi), FE_MEM(FE_BP, 0, 0, -(8 + 8 * idx)), info.val);
            }

            continue;
        }

        assert(var->info.index() == 3);
        const auto *op = std::get<3>(var->info).get();
        assert(op != nullptr);

        std::array<uint64_t, 4> in_regs{};
        size_t arg_count = 0;
        for (size_t in_idx = 0; in_idx < op->in_vars.size(); ++in_idx) {
            const auto &in_var = op->in_vars[in_idx];
            if (!in_var)
                break;

            arg_count++;
            if (in_var->type == Type::mt)
                continue;

            const auto reg = op_reg_mapping[in_idx];

            // zero the full register so stuff doesn't go broke e.g. in zero-extend
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, reg, reg);
            fe_enc64(as.cur_instr_ptr(), opcode(in_var->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), reg, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, in_var))));
            in_regs[in_idx] = reg;
        }

        switch (op->type) {
        case Instruction::store:
            assert(op->in_vars[0]->type == Type::i64 || op->in_vars[0]->type == Type::imm);
            assert(arg_count == 3);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[1]->type, FE_MOV64mr, FE_MOV32mr, FE_MOV16mr, FE_MOV8mr), FE_MEM(in_regs[0], 0, 0, 0), in_regs[1]);
            break;
        case Instruction::load:
            assert(op->in_vars[0]->type == Type::i64 || op->in_vars[0]->type == Type::imm);
            assert(op->out_vars[0] == var);
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), opcode(var->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), in_regs[0], FE_MEM(in_regs[0], 0, 0, 0));
            break;
        case Instruction::add:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_ADD64rr, FE_AX, FE_BX);
            break;
        case Instruction::sub:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_SUB64rr, FE_AX, FE_BX);
            break;
        case Instruction::mul_l:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_IMUL64rr, FE_AX, FE_BX);
            break;
        case Instruction::ssmul_h:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_IMUL64r, FE_BX);
            fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_AX, FE_DX);
            break;
        case Instruction::uumul_h:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_MUL64r, FE_BX);
            fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_AX, FE_DX);
            break;
        case Instruction::div:
            assert(arg_count == 2 || arg_count == 3);
            fe_enc64(as.cur_instr_ptr(), FE_C_SEP64);
            fe_enc64(as.cur_instr_ptr(), FE_IDIV64r, FE_BX);
            fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_BX, FE_DX); // second output is remainder and needs to be in rbx atm
            break;
        case Instruction::udiv:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_DX, FE_DX);
            fe_enc64(as.cur_instr_ptr(), FE_DIV64r, FE_BX);
            fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_BX, FE_DX); // second output is remainder and needs to be in rbx atm
            break;
        case Instruction::shl:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_MOV8rr, FE_CX, FE_BX);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_SHL64rr, FE_SHL32rr, FE_SHL16rr, FE_SHL8rr), FE_AX, FE_CX);
            break;
        case Instruction::shr:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_MOV8rr, FE_CX, FE_BX);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_SHR64rr, FE_SHR32rr, FE_SHR16rr, FE_SHR8rr), FE_AX, FE_CX);
            break;
        case Instruction::sar:
            assert(arg_count == 2);
            // make sure that it uses the bit-width of the input operand for shifting
            // so that the sign-bit is properly recognized
            fe_enc64(as.cur_instr_ptr(), FE_MOV8rr, FE_CX, FE_BX);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_SAR64rr, FE_SAR32rr, FE_SAR16rr, FE_SAR8rr), FE_AX, FE_CX);
            break;
        case Instruction::_or:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_OR64rr, FE_AX, FE_BX);
            break;
        case Instruction::_and:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_AND64rr, FE_AX, FE_BX);
            break;
        case Instruction::_not:
            assert(arg_count == 1);
            fe_enc64(as.cur_instr_ptr(), FE_NOT64r, FE_AX);
            break;
        case Instruction::_xor:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), FE_XOR64rr, FE_AX, FE_BX);
            break;
        case Instruction::cast:
            assert(arg_count == 1);
            break;
        case Instruction::setup_stack:
            assert(arg_count == 0);
            // TODO: the immediate could be too large so put it somewhere in the data section again and use
            // mov rax, offset64
            fe_enc64(as.cur_instr_ptr(), FE_MOV64ra, FE_AX, as.init_stack_ptr);
            break;
        case Instruction::zero_extend:
            assert(arg_count == 1);
            // nothing to be done
            break;
        case Instruction::sign_extend:
            assert(arg_count == 1);
            if (op->in_vars[0]->type != Type::i64) {
                fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, 0, FE_MOVSXr64r32, FE_MOVSXr64r16, FE_MOVSXr64r8), FE_AX, in_regs[0]);
            }
            break;
        case Instruction::slt:
            assert(arg_count == 4);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMP64rr, FE_CMP32rr, FE_CMP16rr, FE_CMP8rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMOVL64rr, FE_CMOVL32rr, FE_CMOVL16rr), in_regs[0], in_regs[2]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMOVGE64rr, FE_CMOVGE32rr, FE_CMOVGE16rr), in_regs[0], in_regs[3]);
            break;
        case Instruction::sltu:
            assert(arg_count == 4);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMP64rr, FE_CMP32rr, FE_CMP16rr, FE_CMP8rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMOVC64rr, FE_CMOVC32rr, FE_CMOVC16rr), in_regs[0], in_regs[2]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMOVNC64rr, FE_CMOVNC32rr, FE_CMOVNC16rr), in_regs[0], in_regs[3]);
            break;
        case Instruction::sumul_h: /* TODO: implement */
            assert(0);
            break;
        case Instruction::umax:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMP64rr, FE_CMP32rr, FE_CMP16rr, FE_CMP8rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMOVA64rr, FE_CMOVA32rr, FE_CMOVA16rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_MOV64rr, FE_MOV32rr, FE_MOV16rr), FE_AX, in_regs[0]);
            break;
        case Instruction::umin:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMP64rr, FE_CMP32rr, FE_CMP16rr, FE_CMP8rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMOVC64rr, FE_CMOVC32rr, FE_CMOVC16rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_MOV64rr, FE_MOV32rr, FE_MOV16rr), FE_AX, in_regs[0]);
            break;
        case Instruction::max:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMP64rr, FE_CMP32rr, FE_CMP16rr, FE_CMP8rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMOVG64rr, FE_CMOVG32rr, FE_CMOVG16rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_MOV64rr, FE_MOV32rr, FE_MOV16rr), FE_AX, in_regs[0]);
            break;
        case Instruction::min:
            assert(arg_count == 2);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMP64rr, FE_CMP32rr, FE_CMP16rr, FE_CMP8rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_CMOVL64rr, FE_CMOVL32rr, FE_CMOVL16rr), in_regs[0], in_regs[1]);
            fe_enc64(as.cur_instr_ptr(), opcode(op->in_vars[0]->type, FE_MOV64rr, FE_MOV32rr, FE_MOV16rr), FE_AX, in_regs[0]);
            break;
        }

        if (var->type != Type::mt) {
            for (size_t out_idx = 0; out_idx < op->out_vars.size(); ++out_idx) {
                const auto &out_var = op->out_vars[out_idx];
                if (!out_var)
                    continue;

                fe_enc64(as.cur_instr_ptr(), opcode(out_var->type, FE_MOV64mr, FE_MOV32mr, FE_MOV16mr, FE_MOV8mr), FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, out_var))),
                         op_reg_mapping[out_idx]);
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

        const auto target_is_static = std::holds_alternative<size_t>(target_var->info);
        if (std::holds_alternative<size_t>(source_var->info)) {
            if (optimizations & OPT_UNUSED_STATIC) {
                if (target_is_static && std::get<size_t>(source_var->info) == std::get<size_t>(target_var->info)) {
                    // TODO: see this as a different optimization?
                    continue;
                } else {
                    // when using the unused static optimization, the static load might have been optimized out
                    // so we need to get the static directly
                    fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
                    as.load_static_in_reg(std::get<size_t>(source_var->info), FE_AX, source_var->type);
                }
            } else {
                fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
                fe_enc64(as.cur_instr_ptr(), opcode(source_var->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), FE_AX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, source_var))));
            }
        } else {
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
            fe_enc64(as.cur_instr_ptr(), opcode(source_var->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), FE_AX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, source_var))));
        }

        if (target_is_static) {
            as.save_static_from_reg(std::get<size_t>(target_var->info), FE_AX, target_var->type);
        } else {
            fe_enc64(as.cur_instr_ptr(), FE_MOV64mr, FE_MEM(FE_R12, 0, 0, 0), FE_AX);
            fe_enc64(as.cur_instr_ptr(), FE_ADD64ri, FE_R12, 8);
        }
    }

    // destroy stack space
    fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_SP, FE_BP);
    fe_enc64(as.cur_instr_ptr(), FE_POPr, FE_BP);
}

void Generator::compile_ret_args(const BasicBlock *block, const CfOp &op) {
    assert(op.info.index() == 2);
    const auto &ret_info = std::get<CfOp::RetInfo>(op.info);
    for (const auto &[var, s_idx] : ret_info.mapping) {
        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            if (std::get<size_t>(var->info) == s_idx) {
                continue;
            }
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
            as.load_static_in_reg(std::get<size_t>(var->info), FE_AX, var->type);
        } else {
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
            fe_enc64(as.cur_instr_ptr(), opcode(var->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), FE_AX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, var))));
        }

        as.save_static_from_reg(s_idx, FE_AX, var->type);
    }

    // destroy stack space
    fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_SP, FE_BP);
    fe_enc64(as.cur_instr_ptr(), FE_POPr, FE_BP);
}

void Generator::compile_cjump(const BasicBlock *block, const CfOp &cf_op, [[maybe_unused]] const size_t cond_idx) {
    assert(cf_op.in_vars[0] != nullptr && cf_op.in_vars[1] != nullptr);
    assert(cf_op.info.index() == 1);
    // this breaks when the arg mapping is changed
    fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
    fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_BX, FE_BX);
    if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(cf_op.in_vars[0]->info)) {
        // load might be optimized out so get the value directly
        as.load_static_in_reg(std::get<size_t>(cf_op.in_vars[0]->info), FE_AX, cf_op.in_vars[0]->type);
    } else {
        fe_enc64(as.cur_instr_ptr(), opcode(cf_op.in_vars[0]->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), FE_AX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, cf_op.in_vars[0]))));
    }
    if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(cf_op.in_vars[1]->info)) {
        // load might be optimized out so get the value directly
        as.load_static_in_reg(std::get<size_t>(cf_op.in_vars[1]->info), FE_BX, cf_op.in_vars[1]->type);
    } else {
        fe_enc64(as.cur_instr_ptr(), opcode(cf_op.in_vars[1]->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), FE_BX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, cf_op.in_vars[1]))));
    }
    fe_enc64(as.cur_instr_ptr(), FE_CMP64rr, FE_AX, FE_BX);

    const auto &info = std::get<CfOp::CJumpInfo>(cf_op.info);
    auto op = 0;
    switch (info.type) {
    case CfOp::CJumpInfo::CJumpType::eq:
        op = FE_JNZ;
        break;
    case CfOp::CJumpInfo::CJumpType::neq:
        op = FE_JZ;
        break;
    case CfOp::CJumpInfo::CJumpType::lt:
        op = FE_JNC;
        break;
    case CfOp::CJumpInfo::CJumpType::gt:
        op = FE_JBE;
        break;
    case CfOp::CJumpInfo::CJumpType::slt:
        op = FE_JGE;
        break;
    case CfOp::CJumpInfo::CJumpType::sgt:
        op = FE_JLE;
        break;
    }
    as.add_short_jmp(op);
    needs_short_jmp_resolve = true;

    compile_cf_args(block, cf_op);
    as.add_jmp_to_bb(info.target->id);
}

void Generator::compile_syscall(const BasicBlock *block, const CfOp &cf_op) {
    const auto &info = std::get<CfOp::SyscallInfo>(cf_op.info);
    compile_continuation_args(block, info.continuation_mapping);

    for (size_t i = 0; i < call_reg.size(); ++i) {
        const auto &var = cf_op.in_vars[i];
        if (!var)
            break;

        if (var->type == Type::mt)
            continue;

        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            as.load_static_in_reg(std::get<size_t>(var->info), call_reg[i], var->type);
        } else {
            fe_enc64(as.cur_instr_ptr(), FE_MOV64rm, call_reg[i], FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, var))));
        }
    }
    if (cf_op.in_vars[6] == nullptr) {
        // fe_enc64(as.cur_instr_ptr(), FE_PUSHi, 0);
        fe_enc64(as.cur_instr_ptr(), FE_SUB64ri, FE_SP, 16);
    } else {
        fe_enc64(as.cur_instr_ptr(), FE_SUB64ri, FE_SP, 8);
        fe_enc64(as.cur_instr_ptr(), FE_MOV64rm, FE_AX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, cf_op.in_vars[6]))));
        fe_enc64(as.cur_instr_ptr(), FE_PUSHr, FE_AX);
    }

    as.add_syscall();
    fe_enc64(as.cur_instr_ptr(), FE_ADD64ri, FE_SP, 16);
    if (info.static_mapping.size() > 0) {
        as.save_static_from_reg(info.static_mapping[0], FE_AX, Type::i64);
    }
    fe_enc64(as.cur_instr_ptr(), FE_MOV64rr, FE_SP, FE_BP);
    fe_enc64(as.cur_instr_ptr(), FE_POPr, FE_BP);
    as.add_jmp_to_bb(info.continuation_block->id);
}

void Generator::compile_continuation_args(const BasicBlock *block, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping) {
    for (const auto &[var, s_idx] : mapping) {
        if (var->type == Type::mt) {
            continue;
        }

        if (optimizations & OPT_UNUSED_STATIC && std::holds_alternative<size_t>(var->info)) {
            const auto orig_static_idx = std::get<size_t>(var->info);
            if (orig_static_idx == s_idx) {
                continue;
            }
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
            as.load_static_in_reg(orig_static_idx, FE_AX, var->type);
        } else {
            fe_enc64(as.cur_instr_ptr(), FE_XOR32rr, FE_AX, FE_AX);
            fe_enc64(as.cur_instr_ptr(), opcode(var->type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), FE_AX, FE_MEM(FE_BP, 0, 0, -(8 + 8 * index_for_var(block, var))));
        }

        as.save_static_from_reg(s_idx, FE_AX, Type::i64);
    }
}
