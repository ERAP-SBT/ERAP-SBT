#include "generator/x86_64/generator.h"

#include <sstream>

using namespace generator::x86_64;

namespace {
// dupe code
std::array<std::array<const char *, 4>, REG_COUNT> reg_names = {
    std::array<const char *, 4>{"rax", "eax", "ax", "al"},
    {"rbx", "ebx", "bx", "bl"},
    {"rcx", "ecx", "cx", "cl"},
    {"rdx", "edx", "dx", "dl"},
    {"rdi", "edi", "di", "dil"},
    {"rsi", "esi", "si", "sil"},
    {"r8", "r8d", "r8w", "r8b"},
    {"r9", "r9d", "r9w", "r9b"},
    {"r10", "r10d", "r10w", "r10b"},
    {"r11", "r11d", "r11w", "r11b"},
    {"r12", "r12d", "r12w", "r12b"},
    {"r13", "r13d", "r13w", "r13b"},
    {"r14", "r14d", "r14w", "r14b"},
    {"r15", "r15d", "r15w", "r15b"},
};

std::array<REGISTER, 6> call_reg = {REG_DI, REG_SI, REG_D, REG_C, REG_8, REG_9};

// only for integers
const char *reg_name(const REGISTER reg, const Type type) {
    const auto &arr = reg_names[reg];
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

const char *mem_size(const Type type) {
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

Type choose_type(Type typ1, Type typ2) {
    assert(typ1 == typ2 || typ1 == Type::imm || typ2 == Type::imm);
    if (typ1 == Type::imm && typ2 == Type::imm) {
        return Type::i64;
    } else if (typ1 == Type::imm) {
        return typ2;
    } else {
        return typ1;
    }
}
} // namespace

void RegAlloc::compile_blocks() {

    for (const auto &bb : gen->ir->basic_blocks) {
        if (bb->gen_info.compiled) {
            continue;
        }

        /*if (!bb->predecessors.empty()) {
            // only compile top-level bbs or blocks that self reference
            auto count = bb->predecessors.size();
            for (auto* pred : bb->predecessors) {
                if (pred == bb.get()) {
                    count--;
                }
            }
            if (count > 0) {
                continue;
            }
        }*/
        if (!is_block_top_level(bb.get())) {
            continue;
        }

        auto supported = true;
        for (auto *input : bb->inputs) {
            if (!std::holds_alternative<size_t>(input->info)) {
                supported = false;
                break;
            }

            const auto static_idx = std::get<size_t>(input->info);
            input->gen_info.location = SSAVar::GeneratorInfoX64::STATIC;
            input->gen_info.static_idx = static_idx;
        }

        if (!supported) {
            // TODO: compile the normal way?
            continue;
        }

        // only for testing
        if (!bb->gen_info.input_map_setup) {
            assert(!bb->gen_info.input_map_setup);
            generate_input_map(bb.get());
        }

        size_t max_stack_frame = 0;
        compile_block(bb.get(), true, max_stack_frame);

        translation_blocks.clear();
        asm_buf.clear();
    }

    // when blocks have a self-contained circular reference chain, they do not get recognized as top-level so
    // we compile them here and use the lowest-id-block (which should later be the lowest-address one) as the first
    for (const auto &bb : gen->ir->basic_blocks) {
        if (bb->gen_info.compiled) {
            continue;
        }

        auto supported = true;
        for (auto *input : bb->inputs) {
            if (!std::holds_alternative<size_t>(input->info)) {
                supported = false;
                break;
            }

            const auto static_idx = std::get<size_t>(input->info);
            input->gen_info.location = SSAVar::GeneratorInfoX64::STATIC;
            input->gen_info.static_idx = static_idx;
        }

        if (!supported) {
            // TODO: compile the normal way?
            continue;
        }

        // only for testing
        if (!bb->gen_info.input_map_setup) {
            assert(!bb->gen_info.input_map_setup);
            generate_input_map(bb.get());
        }

        size_t max_stack_frame = 0;
        bb->gen_info.manual_top_level = true;
        compile_block(bb.get(), true, max_stack_frame);

        translation_blocks.clear();
        asm_buf.clear();
    }
}

constexpr size_t BB_OLD_COMPILE_ID_TIL = static_cast<size_t>(-1);
constexpr size_t BB_MERGE_TIL_ID = static_cast<size_t>(-1);

void RegAlloc::compile_block(BasicBlock *bb, const bool first_block, size_t &max_stack_frame_size) {
    RegMap reg_map = {};
    StackMap stack_map = {};
    // TODO: use some sort of struct with a destructor to reset these on return?
    cur_bb = bb;
    cur_reg_map = &reg_map;
    cur_stack_map = &stack_map;

    // TODO: this hack is needed because syscalls have a continuation mapping so we cant create the input mapping in the previous' block
    // cfop
    if (!bb->gen_info.input_map_setup) {
        for (auto *input : bb->inputs) {
            if (!std::holds_alternative<size_t>(input->info)) {
                assert(0);
                exit(1);
                return;
            }

            const auto static_idx = std::get<size_t>(input->info);
            input->gen_info.location = SSAVar::GeneratorInfoX64::STATIC;
            input->gen_info.static_idx = static_idx;
            BasicBlock::GeneratorInfo::InputInfo info;
            info.location = BasicBlock::GeneratorInfo::InputInfo::STATIC;
            info.static_idx = static_idx;
            bb->gen_info.input_map.push_back(std::move(info));
        }
        bb->gen_info.input_map_setup = true;
    }

    if (bb->id > BB_OLD_COMPILE_ID_TIL) {
        gen->compile_block_reg_alloc(bb);
        bb->gen_info.compiled = true;
    } else {

        // fill in reg_map and stack_map from inputs
        for (auto *var : bb->inputs) {
            if (var->gen_info.location == SSAVar::GeneratorInfoX64::REGISTER) {
                reg_map[var->gen_info.reg_idx].cur_var = var;
                reg_map[var->gen_info.reg_idx].alloc_time = 0;
            } else if (var->gen_info.location == SSAVar::GeneratorInfoX64::STACK_FRAME) {
                const auto stack_slot = var->gen_info.stack_slot;
                if (stack_map.size() <= stack_slot) {
                    stack_map.resize(stack_slot + 1);
                }
                stack_map[stack_slot].free = false;
                stack_map[stack_slot].var = var;
            }
        }

        if (!first_block) {
            generate_translation_block(bb);
            print_asm("b%zu_reg_alloc:\n", bb->id);
            print_asm("# MBRA\n"); // multi-block register allocation
            print_asm("# Virt Start: %#lx\n# Virt End:  %#lx\n", bb->virt_start_addr, bb->virt_end_addr);
        }

        init_time_of_use(bb);

        compile_vars(bb);

        prepare_cf_ops(bb);
        {
            auto asm_block = AssembledBlock{};
            asm_block.bb = bb;
            asm_block.assembly = asm_buf;
            asm_buf.clear();
            asm_block.reg_map = reg_map;
            asm_block.stack_map = std::move(stack_map);
            assembled_blocks.push_back(std::move(asm_block));
        }

        cur_bb = nullptr;
        cur_stack_map = nullptr;
        cur_reg_map = nullptr;
        bb->gen_info.compiled = true;

        max_stack_frame_size = std::max(max_stack_frame_size, stack_map.size());
        // TODO: prioritize jumps so we can omit the jmp bX_reg_alloc
        for (const auto &cf_op : bb->control_flow_ops) {
            auto *target = cf_op.target();
            if (target && !target->gen_info.compiled && target->id <= BB_MERGE_TIL_ID) {
                compile_block(target, false, max_stack_frame_size);
            }
        }

        if (first_block) {
            // need to add a bit of space to the stack since the cfops might need to spill to the stack
            max_stack_frame_size += gen->ir->statics.size();
            // align to 16 bytes
            max_stack_frame_size = ((max_stack_frame_size + 15) & 0xFFFFFFFF'FFFFFFF0);

            fprintf(gen->out_fd, "b%zu:\nsub rsp, %zu\n", bb->id, max_stack_frame_size * 8);
            fprintf(gen->out_fd, "# MBRA\n"); // multi-block register allocation
            fprintf(gen->out_fd, "# Virt Start: %#lx\n# Virt End:  %#lx\n", bb->virt_start_addr, bb->virt_end_addr);
            write_assembled_blocks(max_stack_frame_size);

            fprintf(gen->out_fd, "\n# Translation Blocks\n");
            for (const auto &pair : translation_blocks) {
                fprintf(gen->out_fd, "b%zu:\nsub rsp, %zu\n", pair.first, max_stack_frame_size * 8);
                fprintf(gen->out_fd, "# MBRATB\n"); // multi-block register allocation translation block
                fprintf(gen->out_fd, "%s\n", pair.second.c_str());
            }
            fprintf(gen->out_fd, "\n");
            translation_blocks.clear();
            assembled_blocks.clear();
        }
    }
}

void RegAlloc::compile_vars(BasicBlock *bb) {
    auto &reg_map = *cur_reg_map;
    std::ostringstream ir_stream;
    for (size_t var_idx = 0; var_idx < bb->variables.size(); ++var_idx) {
        auto *var = bb->variables[var_idx].get();
        const auto cur_time = var_idx;

        // print ir for better readability
        // TODO: add flag for this to reduce useless stuff in asm file
        // TODO: print ir
        // TODO: when we merge ops, we need to print ir before the merged op
        ir_stream.str("");
        var->print(ir_stream, gen->ir);
        print_asm("# %s\n", ir_stream.str().c_str());

        if (var->gen_info.already_generated) {
            continue;
        }

        // TODO: this essentially skips input vars but we should have a seperate if for that
        // since the location of the input vars is supplied by the previous block
        if (var->type == Type::imm || std::holds_alternative<size_t>(var->info)) {
            // skip immediates and statics, we load them on-demand
            continue;
        }

        if (!std::holds_alternative<std::unique_ptr<Operation>>(var->info)) {
            // skip vars that depend on ops of other vars for example
            continue;
        }

        auto *op = std::get<std::unique_ptr<Operation>>(var->info).get();
        switch (op->type) {
        case Instruction::add:
            [[fallthrough]];
        case Instruction::sub:
            [[fallthrough]];
        case Instruction::shl:
            [[fallthrough]];
        case Instruction::shr:
            [[fallthrough]];
        case Instruction::sar:
            [[fallthrough]];
        case Instruction::_or:
            [[fallthrough]];
        case Instruction::_and:
            [[fallthrough]];
        case Instruction::_xor:
            [[fallthrough]];
        case Instruction::umax:
            [[fallthrough]];
        case Instruction::umin:
            [[fallthrough]];
        case Instruction::max:
            [[fallthrough]];
        case Instruction::min:
            [[fallthrough]];
        case Instruction::mul_l:
            [[fallthrough]];
        case Instruction::ssmul_h:
            [[fallthrough]];
        case Instruction::uumul_h:
            [[fallthrough]];
        case Instruction::div:
            [[fallthrough]];
        case Instruction::udiv: {
            auto *in1 = op->in_vars[0].get();
            auto *in2 = op->in_vars[1].get();
            auto *dst = op->out_vars[0];
            // TODO: the imm-branch and not-imm-branch can probably be merged if we use a string as the second operand
            // and just put the imm in there
            // TODO: with two immediates we could constant evaluate but that should be taken care of by the IR
            // TODO: when there are two immediates and one of them is binary relative we can optimize that into
            // one lea reg, [binary + in1 + in2] if it can be assembled
            if (in2->type == Type::imm && !std::get<SSAVar::ImmInfo>(in2->info).binary_relative) {
                const auto imm_val = std::get<SSAVar::ImmInfo>(in2->info).val;
                REGISTER in1_reg;
                if (op->type != Instruction::ssmul_h && op->type != Instruction::uumul_h && op->type != Instruction::div && op->type != Instruction::udiv) {
                    in1_reg = load_val_in_reg(cur_time, in1);
                } else {
                    // need to load into rax and clear rdx
                    in1_reg = load_val_in_reg(cur_time, in1, REG_A);

                    // rdx gets clobbered by mul and used by div
                    if (reg_map[REG_D].cur_var && reg_map[REG_D].cur_var->gen_info.last_use_time > cur_time) {
                        save_reg(REG_D);
                    }
                    clear_reg(cur_time, REG_D);
                    if (op->type == Instruction::div && op->type == Instruction::udiv) {
                        // TODO: theoretically this could go below but having it up here should actually be better for the pipeline, no?
                        print_asm("cqo\n");
                    }
                }

                const auto in1_reg_name = reg_name(in1_reg, choose_type(in1->type, Type::imm));
                REGISTER dst_reg = REG_NONE;
                if (op->type != Instruction::add || in1->gen_info.last_use_time == cur_time) {
                    dst_reg = in1_reg;
                } else {
                    // check if there is a free register
                    for (size_t reg = 0; reg < REG_COUNT; ++reg) {
                        if (reg_map[reg].cur_var == nullptr) {
                            dst_reg = static_cast<REGISTER>(reg);
                            break;
                        }
                        auto *var = reg_map[reg].cur_var;
                        if (var->gen_info.last_use_time < cur_time) {
                            dst_reg = static_cast<REGISTER>(reg);
                            break;
                        }
                    }
                    if (dst_reg == REG_NONE) {
                        size_t in1_next_use = 0;
                        for (auto use : in1->gen_info.uses) {
                            if (use > cur_time) {
                                in1_next_use = use;
                                break;
                            }
                        }

                        // check if there is a variable thats already saved on the stack and used after the dst
                        auto check_unsaved_vars = !in1->gen_info.saved_in_stack;
                        for (size_t reg = 0; reg < REG_COUNT; ++reg) {
                            auto *var = reg_map[reg].cur_var;
                            if (!check_unsaved_vars && !var->gen_info.saved_in_stack) {
                                continue;
                            }
                            size_t var_next_use = 0;
                            for (auto use : var->gen_info.uses) {
                                if (use > cur_time) {
                                    var_next_use = use;
                                    break;
                                }
                            }
                            if (var_next_use > in1_next_use) {
                                dst_reg = static_cast<REGISTER>(reg);
                                break;
                            }
                        }

                        if (dst_reg == REG_NONE) {
                            dst_reg = in1_reg;
                        }
                    }
                }
                const auto dst_reg_name = reg_name(dst_reg, choose_type(in1->type, Type::imm));

                if (reg_map[dst_reg].cur_var && reg_map[dst_reg].cur_var->gen_info.last_use_time > cur_time) {
                    save_reg(dst_reg);
                }

                // TODO: optimization flag for merging?
                auto did_merge = false;
                if (dst->ref_count == 1 && op->type == Instruction::add && bb->variables.size() > var_idx + 1) {
                    // check if next instruction is a load
                    auto *next_var = bb->variables[var_idx + 1].get();
                    if (std::holds_alternative<std::unique_ptr<Operation>>(next_var->info)) {
                        // TODO: dont shadow local var
                        auto *op = std::get<std::unique_ptr<Operation>>(next_var->info).get();
                        if (op->type == Instruction::load && op->in_vars[0] == dst) {
                            auto *load_dst = op->out_vars[0];
                            // check for zero/sign-extend
                            if (load_dst->ref_count == 1 && bb->variables.size() > var_idx + 2) {
                                auto *nnext_var = bb->variables[var_idx + 2].get();
                                if (std::holds_alternative<std::unique_ptr<Operation>>(nnext_var->info)) {
                                    auto *nop = std::get<std::unique_ptr<Operation>>(nnext_var->info).get();
                                    if (nop->type == Instruction::zero_extend) {
                                        auto *ext_dst = nop->out_vars[0];
                                        if (load_dst->type == Type::i32) {
                                            print_asm("mov %s, [%s + %ld]\n", reg_names[dst_reg][1], in1_reg_name, imm_val);
                                        } else {
                                            print_asm("movzx %s, %s [%s + %ld]\n", reg_name(dst_reg, ext_dst->type), mem_size(load_dst->type), in1_reg_name, imm_val);
                                        }
                                        clear_reg(cur_time, dst_reg);
                                        set_var_to_reg(cur_time, ext_dst, dst_reg);
                                        load_dst->gen_info.already_generated = true;
                                        ext_dst->gen_info.already_generated = true;
                                        did_merge = true;
                                    } else if (nop->type == Instruction::sign_extend) {
                                        auto *ext_dst = nop->out_vars[0];
                                        if (load_dst->type == Type::i32) {
                                            assert(ext_dst->type == Type::i32 || ext_dst->type == Type::i64);
                                            print_asm("movsxd %s, %s [%s + %ld]\n", reg_name(dst_reg, ext_dst->type), mem_size(load_dst->type), in1_reg_name, imm_val);
                                        } else {
                                            print_asm("movsx %s, %s [%s + %ld]\n", reg_name(dst_reg, ext_dst->type), mem_size(load_dst->type), in1_reg_name, imm_val);
                                        }
                                        clear_reg(cur_time, dst_reg);
                                        set_var_to_reg(cur_time, ext_dst, dst_reg);
                                        load_dst->gen_info.already_generated = true;
                                        ext_dst->gen_info.already_generated = true;
                                        did_merge = true;
                                    }
                                }
                            }

                            if (!did_merge) {
                                // merge add and load
                                print_asm("mov %s, [%s + %ld]\n", reg_name(dst_reg, load_dst->type), in1_reg_name, imm_val);
                                clear_reg(cur_time, dst_reg);
                                set_var_to_reg(cur_time, load_dst, dst_reg);
                                load_dst->gen_info.already_generated = true;
                                did_merge = true;
                            }
                        } else if (op->type == Instruction::cast) {
                            // detect add/cast/store sequence
                            auto *cast_var = op->out_vars[0];
                            if (cast_var->ref_count == 1 && bb->variables.size() > var_idx + 2) {
                                auto *nnext_var = bb->variables[var_idx + 2].get();
                                if (std::holds_alternative<std::unique_ptr<Operation>>(nnext_var->info)) {
                                    auto *nop = std::get<std::unique_ptr<Operation>>(nnext_var->info).get();
                                    if (nop->type == Instruction::store && nop->in_vars[0] == dst && nop->in_vars[1] == cast_var) {
                                        auto *store_dst = nop->out_vars[0]; // should be == nnext_var
                                        // load source of cast
                                        const auto cast_reg = load_val_in_reg(cur_time, op->in_vars[0].get());
                                        print_asm("mov [%s + %ld], %s\n", in1_reg_name, imm_val, reg_name(cast_reg, cast_var->type));
                                        cast_var->gen_info.already_generated = true;
                                        store_dst->gen_info.already_generated = true;
                                        did_merge = true;
                                    }
                                }
                            }
                        } else if (op->type == Instruction::store && op->in_vars[0].get() == dst) {
                            // merge add/store
                            auto *store_src = op->in_vars[1].get();
                            const auto src_reg = load_val_in_reg(cur_time, store_src);
                            print_asm("mov [%s + %ld], %s\n", in1_reg_name, imm_val, reg_name(src_reg, store_src->type));
                            op->out_vars[0]->gen_info.already_generated = true;
                            did_merge = true;
                        }
                    }
                }
                if (did_merge) {
                    break;
                }

                switch (op->type) {
                case Instruction::add:
                    if (dst_reg == in1_reg) {
                        print_asm("add %s, %ld\n", in1_reg_name, imm_val);
                    } else {
                        print_asm("lea %s, [%s + %ld]\n", dst_reg_name, in1_reg_name, imm_val);
                    }
                    break;
                case Instruction::sub:
                    print_asm("sub %s, %ld\n", in1_reg_name, imm_val);
                    break;
                case Instruction::shl:
                    print_asm("shl %s, %ld\n", in1_reg_name, imm_val);
                    break;
                case Instruction::shr:
                    print_asm("shr %s, %ld\n", in1_reg_name, imm_val);
                    break;
                case Instruction::sar:
                    print_asm("sar %s, %ld\n", in1_reg_name, imm_val);
                    break;
                case Instruction::_or:
                    print_asm("or %s, %ld\n", in1_reg_name, imm_val);
                    break;
                case Instruction::_and:
                    print_asm("and %s, %ld\n", in1_reg_name, imm_val);
                    break;
                case Instruction::_xor:
                    print_asm("xor %s, %ld\n", in1_reg_name, imm_val);
                    break;
                case Instruction::umax:
                    print_asm("cmp %s, %ld\n", in1_reg_name, imm_val);
                    print_asm("jae b%zu_%zu_max\n", bb->id, var_idx);
                    print_asm("mov %s, %ld\n", in1_reg_name, imm_val);
                    print_asm("b%zu_%zu_max:\n", bb->id, var_idx);
                    break;
                case Instruction::umin:
                    print_asm("cmp %s, %ld\n", in1_reg_name, imm_val);
                    print_asm("jbe b%zu_%zu_min\n", bb->id, var_idx);
                    print_asm("mov %s, %ld\n", in1_reg_name, imm_val);
                    print_asm("b%zu_%zu_min:\n", bb->id, var_idx);
                    break;
                case Instruction::max:
                    print_asm("cmp %s, %ld\n", in1_reg_name, imm_val);
                    print_asm("jge b%zu_%zu_smax\n", bb->id, var_idx);
                    print_asm("mov %s, %ld\n", in1_reg_name, imm_val);
                    print_asm("b%zu_%zu_smax:\n", bb->id, var_idx);
                    break;
                case Instruction::min:
                    print_asm("cmp %s, %ld\n", in1_reg_name, imm_val);
                    print_asm("jle b%zu_%zu_smin\n", bb->id, var_idx);
                    print_asm("mov %s, %ld\n", in1_reg_name, imm_val);
                    print_asm("b%zu_%zu_smin:\n", bb->id, var_idx);
                    break;
                case Instruction::mul_l:
                    assert(static_cast<uint64_t>(imm_val) <= 0xFFFFFFFF);
                    print_asm("imul %s, %ld\n", in1_reg_name, imm_val);
                    break;
                case Instruction::ssmul_h: {
                    const auto imm_reg = load_val_in_reg(cur_time, in2);
                    print_asm("imul %s\n", reg_name(imm_reg, in1->type));
                    break;
                }
                case Instruction::uumul_h: {
                    const auto imm_reg = load_val_in_reg(cur_time, in2);
                    print_asm("mul %s\n", reg_name(imm_reg, in1->type));
                    break;
                }
                case Instruction::div: {
                    const auto imm_reg = load_val_in_reg(cur_time, in2, REG_NONE, REG_D);
                    print_asm("idiv %s\n", reg_name(imm_reg, in1->type));
                    break;
                }
                case Instruction::udiv: {
                    const auto imm_reg = load_val_in_reg(cur_time, in2, REG_NONE, REG_D);
                    print_asm("div %s\n", reg_name(imm_reg, in1->type));
                    break;
                }
                default:
                    // should never be hit
                    assert(0);
                    exit(1);
                }

                clear_reg(cur_time, dst_reg);

                if (op->type == Instruction::ssmul_h || op->type == Instruction::uumul_h) {
                    // result is in rdx
                    set_var_to_reg(cur_time, dst, REG_D);
                } else if (op->type == Instruction::div || op->type == Instruction::udiv) {
                    if (dst) {
                        set_var_to_reg(cur_time, dst, REG_A);
                    }
                    if (op->out_vars[1]) {
                        set_var_to_reg(cur_time, op->out_vars[1], REG_D);
                    }
                } else {
                    set_var_to_reg(cur_time, dst, dst_reg);
                }
                break;
            }
            // TODO: when in1 == imm & in2 != imm and we have a sub we can neg in2, add in2, in1
            REGISTER in1_reg, in2_reg;
            if (op->type == Instruction::ssmul_h || op->type == Instruction::uumul_h) {
                // TODO: here we could optimise and accept either in1 or in2 in reg a if one of them already is or is already in a register
                in1_reg = load_val_in_reg(cur_time, in1, REG_A);
                // rdx gets clobbered but it's fine to use it as a source operand
                in2_reg = load_val_in_reg(cur_time, in2);
                if (reg_map[REG_D].cur_var && reg_map[REG_D].cur_var->gen_info.last_use_time > cur_time) {
                    save_reg(REG_D);
                }
                clear_reg(cur_time, REG_D);
            } else if (op->type == Instruction::div || op->type == Instruction::udiv) {
                in1_reg = load_val_in_reg(cur_time, in1, REG_A);
                // div uses rdx so we cannot have a source in it
                in2_reg = load_val_in_reg(cur_time, in2, REG_NONE, REG_D);
                if (reg_map[REG_D].cur_var && reg_map[REG_D].cur_var->gen_info.last_use_time > cur_time) {
                    save_reg(REG_D);
                }
                clear_reg(cur_time, REG_D);
                print_asm("cqo\n");
            } else if (op->type == Instruction::shl || op->type == Instruction::shr || op->type == Instruction::sar) {
                // shift only allows the amount to be in cl
                in2_reg = load_val_in_reg(cur_time, in2, REG_C);
                in1_reg = load_val_in_reg(cur_time, in1);
            } else {
                in1_reg = load_val_in_reg(cur_time, in1);
                in2_reg = load_val_in_reg(cur_time, in2);
            }
            const auto type = choose_type(in1->type, in2->type);
            const auto in1_reg_name = reg_name(in1_reg, type);
            const auto in2_reg_name = reg_name(in2_reg, type);

            if (in1->gen_info.last_use_time > cur_time) {
                save_reg(in1_reg);
            }

            switch (op->type) {
            case Instruction::add:
                print_asm("add %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::sub:
                print_asm("sub %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::shl:
                print_asm("shl %s, cl\n", in1_reg_name);
                break;
            case Instruction::shr:
                print_asm("shr %s, cl\n", in1_reg_name);
                break;
            case Instruction::sar:
                print_asm("sar %s, cl\n", in1_reg_name);
                break;
            case Instruction::_or:
                print_asm("or %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::_and:
                print_asm("and %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::_xor:
                print_asm("xor %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::umax:
                print_asm("cmp %s, %s\n", in1_reg_name, in2_reg_name);
                print_asm("cmovb %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::umin:
                print_asm("cmp %s, %s\n", in1_reg_name, in2_reg_name);
                print_asm("cmova %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::max:
                print_asm("cmp %s, %s\n", in1_reg_name, in2_reg_name);
                print_asm("cmovl %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::min:
                print_asm("cmp %s, %s\n", in1_reg_name, in2_reg_name);
                print_asm("cmovg %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::mul_l:
                print_asm("imul %s, %s\n", in1_reg_name, in2_reg_name);
                break;
            case Instruction::ssmul_h:
                print_asm("imul %s\n", in2_reg_name);
                break;
            case Instruction::uumul_h:
                print_asm("mul %s\n", in2_reg_name);
                break;
            case Instruction::div:
                print_asm("idiv %s\n", in2_reg_name);
                break;
            case Instruction::udiv:
                print_asm("div %s\n", in2_reg_name);
                break;
            default:
                // should never be hit
                assert(0);
                exit(1);
            }

            clear_reg(cur_time, in1_reg);

            if (op->type == Instruction::ssmul_h || op->type == Instruction::uumul_h) {
                // result is in rdx
                set_var_to_reg(cur_time, dst, REG_D);
            } else if (op->type == Instruction::div || op->type == Instruction::udiv) {
                if (dst) {
                    set_var_to_reg(cur_time, dst, REG_A);
                }
                if (op->out_vars[1]) {
                    set_var_to_reg(cur_time, op->out_vars[1], REG_D);
                }
            } else {
                set_var_to_reg(cur_time, dst, in1_reg);
            }
            break;
        }
        case Instruction::load: {
            auto *addr = op->in_vars[0].get();
            auto *dst = op->out_vars[0];

            // TODO: when addr is a (binary-relative) immediate it should be foldable into one instruction
            const auto addr_reg = load_val_in_reg(cur_time, addr);
            if (addr->gen_info.last_use_time > cur_time) {
                save_reg(addr_reg);
            }

            print_asm("mov %s, [%s]\n", reg_name(addr_reg, dst->type), reg_names[addr_reg][0]);
            clear_reg(cur_time, addr_reg);
            set_var_to_reg(cur_time, dst, addr_reg);
            break;
        }
        case Instruction::store: {
            auto *addr = op->in_vars[0].get();
            auto *val = op->in_vars[1].get();
            assert(addr->type == Type::imm || addr->type == Type::i64);

            // TODO: when addr is a (binary-relative) immediate this can be omitted sometimes
            const auto addr_reg = load_val_in_reg(cur_time, addr);
            // TODO: when val is a non-binary-relative immediate this can be omitted sometimes (if val <= 0xFFFFFFFF)
            const auto val_reg = load_val_in_reg(cur_time, val);
            print_asm("mov [%s], %s\n", reg_names[addr_reg][0], reg_name(val_reg, val->type == Type::imm ? Type::i64 : val->type));
            break;
        }
        case Instruction::_not: {
            auto *val = op->in_vars[0].get();
            auto *dst = op->out_vars[0];
            assert(val->type == dst->type || val->type == Type::imm);
            const auto val_reg = load_val_in_reg(cur_time, val);

            if (val->gen_info.last_use_time > cur_time) {
                save_reg(val_reg);
            }

            print_asm("not %s\n", reg_name(val_reg, val->type == Type::imm ? Type::i64 : val->type));
            clear_reg(cur_time, val_reg);
            set_var_to_reg(cur_time, dst, val_reg);
            break;
        }
        case Instruction::slt:
            [[fallthrough]];
        case Instruction::sltu: {
            auto *cmp1 = op->in_vars[0].get();
            auto *cmp2 = op->in_vars[1].get();
            auto *val1 = op->in_vars[2].get();
            auto *val2 = op->in_vars[3].get();
            auto *dst = op->out_vars[0];

            // TODO: optimize for case where val1 = 1 and val2 = 0
            // if val1 or val2 are immediates we dont necessarily need registers for them
            // though we need to use a jmp instead of two cmovs which is slower
            // so only do that when we dont have free/unused registers?
            // TODO: we can also reuse the cmp1,cmp2 registers when cmp1 or cmp2 are no longer needed
            // like so:
            // cmp reg1, reg2
            // mov reg1, val1
            // mov reg2, val2
            // cmovae reg1, reg2

            const auto cmp1_reg = load_val_in_reg(cur_time, cmp1);
            const auto val1_reg = load_val_in_reg(cur_time, val1);
            const auto val2_reg = load_val_in_reg(cur_time, val2);

            if (cmp1->gen_info.last_use_time > cur_time) {
                save_reg(cmp1_reg);
            }

            if (cmp2->type == Type::imm && !std::get<SSAVar::ImmInfo>(cmp2->info).binary_relative) {
                print_asm("cmp %s, %ld\n", reg_name(cmp1_reg, cmp1->type), std::get<SSAVar::ImmInfo>(cmp2->info).val);
            } else {
                const auto cmp2_reg = load_val_in_reg(cur_time, cmp2);
                auto type = choose_type(cmp1->type, cmp2->type);
                print_asm("cmp %s, %s\n", reg_name(cmp1_reg, type), reg_name(cmp2_reg, type));
            }

            if (op->type == Instruction::slt) {
                print_asm("cmovl %s, %s\n", reg_name(cmp1_reg, dst->type), reg_name(val1_reg, dst->type));
                print_asm("cmovge %s, %s\n", reg_name(cmp1_reg, dst->type), reg_name(val2_reg, dst->type));
            } else {
                print_asm("cmovb %s, %s\n", reg_name(cmp1_reg, dst->type), reg_name(val1_reg, dst->type));
                print_asm("cmovae %s, %s\n", reg_name(cmp1_reg, dst->type), reg_name(val2_reg, dst->type));
            }

            clear_reg(cur_time, cmp1_reg);
            set_var_to_reg(cur_time, dst, cmp1_reg);
            break;
        }
        case Instruction::setup_stack: {
            auto *dst = op->out_vars[0];
            assert(dst->type == Type::i64);
            const auto dst_reg = alloc_reg(cur_time);
            print_asm("mov %s, [init_stack_ptr]\n", reg_names[dst_reg][0]);
            set_var_to_reg(cur_time, dst, dst_reg);
            break;
        }
        case Instruction::cast:
            [[fallthrough]];
        case Instruction::sign_extend:
            [[fallthrough]];
        case Instruction::zero_extend: {
            auto *input = op->in_vars[0].get();
            auto *output = op->out_vars[0];
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
                const auto dst_reg = alloc_reg(cur_time);
                const auto dst_reg_name = reg_name(dst_reg, output->type);
                print_asm("mov %s, %ld\n", dst_reg_name, imm);

                set_var_to_reg(cur_time, output, dst_reg);
                break;
            }

            const auto dst_reg = load_val_in_reg(cur_time, input);
            const auto dst_reg_name = reg_name(dst_reg, input->type);
            if (input->gen_info.last_use_time > cur_time) {
                save_reg(dst_reg);
            }

            // TODO: in theory you could simply alias the input var for cast and zero_extend
            if (op->type == Instruction::sign_extend) {
                print_asm("movsx %s, %s\n", reg_name(dst_reg, output->type), dst_reg_name);
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
                    print_asm("mov %s, %s\n", name, name);
                } else {
                    if (type == Type::i16) {
                        print_asm("and %s, 0xFFFF\n", reg_names[dst_reg][0]);
                    } else if (type == Type::i8) {
                        print_asm("and %s, 0xFF\n", reg_names[dst_reg][0]);
                    }
                    // nothing to do for 64 bit
                }
            }

            clear_reg(cur_time, dst_reg);
            set_var_to_reg(cur_time, output, dst_reg);
            break;
        }
        default:
            fprintf(stderr, "Encountered unknown instruction in generator\n");
            assert(0);
            exit(1);
        }
    }
}

void RegAlloc::prepare_cf_ops(BasicBlock *bb) {
    // just set the input maps when the cf-op targets do not yet have a input mapping
    for (auto &cf_op : bb->control_flow_ops) {
        auto *target = cf_op.target();
        if (!target || target->gen_info.input_map_setup) {
            continue;
        }
        switch (cf_op.type) {
        case CFCInstruction::jump:
            set_bb_inputs(target, std::get<CfOp::JumpInfo>(cf_op.info).target_inputs);
            break;
        case CFCInstruction::cjump:
            set_bb_inputs(target, std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs);
            break;
        case CFCInstruction::syscall:
            // TODO: we don't need this, just need to respect the clobbered registers from a syscall
            set_bb_inputs_from_static(target);
            break;
        default:
            break;
        }
    }
}

void RegAlloc::compile_cf_ops(BasicBlock *bb, RegMap &reg_map, StackMap &stack_map, size_t max_stack_frame_size) {
    // TODO: when there is one cfop and it's a jump we can already omit the jmp bX_reg_alloc if the block isn't compiled yet
    // since it will get compiled straight after

    // really scuffed
    // TODO: we already have these one level up
    auto reg_map_bak = reg_map;
    auto stack_map_bak = stack_map;
    std::vector<SSAVar::GeneratorInfoX64> gen_infos;
    for (auto &var : bb->variables) {
        gen_infos.emplace_back(var->gen_info);
    }

    for (size_t cf_idx = 0; cf_idx < bb->control_flow_ops.size(); ++cf_idx) {
        auto cur_time = bb->variables.size();
        // clear allocations from previous cfop since they dont exist here
        // clear_after_alloc_time(cur_time);
        if (cf_idx != 0) {
            reg_map = reg_map_bak;
            stack_map = stack_map_bak;
            for (size_t i = 0; i < bb->variables.size(); ++i) {
                bb->variables[i]->gen_info = gen_infos[i];
            }
        }
        const auto &cf_op = bb->control_flow_ops[cf_idx];
        print_asm("b%zu_reg_alloc_cf%zu:\n", bb->id, cf_idx);

        if (cf_op.type == CFCInstruction::cjump) {
            auto *cmp1 = cf_op.in_vars[0].get();
            auto *cmp2 = cf_op.in_vars[1].get();

            const auto cmp1_reg = load_val_in_reg(cur_time, cmp1);
            const auto type = choose_type(cmp1->type, cmp2->type);
            if (cmp2->type == Type::imm && !std::get<SSAVar::ImmInfo>(cmp2->info).binary_relative) {
                // TODO: only 32bit immediates which are safe to sign extend
                print_asm("cmp %s, %ld\n", reg_name(cmp1_reg, type), std::get<SSAVar::ImmInfo>(cmp2->info).val);
            } else {
                const auto cmp2_reg = load_val_in_reg(cur_time, cmp2);
                print_asm("cmp %s, %s\n", reg_name(cmp1_reg, type), reg_name(cmp2_reg, type));
            }

            // TODO: this can be done a lot more selectively
            gen_infos.clear();
            reg_map_bak = reg_map;
            stack_map_bak = stack_map;
            for (auto &var : bb->variables) {
                gen_infos.emplace_back(var->gen_info);
            }

            switch (std::get<CfOp::CJumpInfo>(cf_op.info).type) {
            case CfOp::CJumpInfo::CJumpType::eq:
                print_asm("jne b%zu_reg_alloc_cf%zu\n", bb->id, cf_idx + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::neq:
                print_asm("je b%zu_reg_alloc_cf%zu\n", bb->id, cf_idx + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::lt:
                print_asm("jae b%zu_reg_alloc_cf%zu\n", bb->id, cf_idx + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::gt:
                print_asm("jbe b%zu_reg_alloc_cf%zu\n", bb->id, cf_idx + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::slt:
                print_asm("jge b%zu_reg_alloc_cf%zu\n", bb->id, cf_idx + 1);
                break;
            case CfOp::CJumpInfo::CJumpType::sgt:
                print_asm("jle b%zu_reg_alloc_cf%zu\n", bb->id, cf_idx + 1);
                break;
            }
        }

        auto target_top_level = false;
        if (auto *target = cf_op.target(); target != nullptr) {
            target_top_level = is_block_top_level(target);
        }
        switch (cf_op.type) {
        case CFCInstruction::jump: {
            write_target_inputs(cf_op.target(), cur_time, std::get<CfOp::JumpInfo>(cf_op.info).target_inputs);
            if (target_top_level) {
                print_asm("# destroy stack space\n");
                print_asm("add rsp, %zu\n", max_stack_frame_size * 8);
                print_asm("jmp b%zu\n", cf_op.target()->id);
            } else {
                print_asm("jmp b%zu_reg_alloc\n", cf_op.target()->id);
            }
            break;
        }
        case CFCInstruction::cjump: {
            write_target_inputs(cf_op.target(), cur_time, std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs);
            if (target_top_level) {
                print_asm("# destroy stack space\n");
                print_asm("add rsp, %zu\n", max_stack_frame_size * 8);
                print_asm("jmp b%zu\n", cf_op.target()->id);
            } else {
                print_asm("jmp b%zu_reg_alloc\n", cf_op.target()->id);
            }
            break;
        }
        case CFCInstruction::unreachable: {
            gen->err_msgs.emplace_back(Generator::ErrType::unreachable, bb);
            print_asm("lea rdi, [rip + err_unreachable_b%zu]\n", bb->id);
            print_asm("jmp panic\n");
            break;
        }
        case CFCInstruction::ijump: {
            const auto &info = std::get<CfOp::IJumpInfo>(cf_op.info);
            write_static_mapping(info.target, cur_time, info.mapping);
            // TODO: we get a problem if the dst is in a static that has already been written out (so overwritten)
            auto *dst = cf_op.in_vars[0].get();
            const auto dst_reg = load_val_in_reg(cur_time + 1 + info.mapping.size(), dst);
            const auto tmp_reg = alloc_reg(cur_time + 1 + info.mapping.size(), REG_NONE, dst_reg);
            const auto dst_reg_name = reg_names[dst_reg][0];
            const auto tmp_reg_name = reg_names[tmp_reg][0];
            assert(dst->type == Type::imm || dst->type == Type::i64);
            print_asm("# destroy stack space\n");
            print_asm("add rsp, %zu\n", max_stack_frame_size * 8);

            gen->err_msgs.emplace_back(Generator::ErrType::unresolved_ijump, bb);

            /* we trust the lifter that the ijump destination is already aligned */

            /* turn absolute address into relative offset from start of first basicblock */
            print_asm("sub %s, %zu\n", dst_reg_name, gen->ir->virt_bb_start_addr);

            print_asm("cmp %s, ijump_lookup_end - ijump_lookup\n", dst_reg_name);
            print_asm("ja 0f\n");
            print_asm("lea %s, [rip + ijump_lookup]\n", tmp_reg_name);
            print_asm("mov %s, [%s + 4 * %s]\n", tmp_reg_name, tmp_reg_name, dst_reg_name);
            print_asm("test %s, %s\n", tmp_reg_name, tmp_reg_name);
            print_asm("je 0f\n");
            print_asm("jmp %s\n", tmp_reg_name);
            print_asm("0:\n");
            print_asm("lea rdi, [rip + err_unresolved_ijump_b%zu]\n", bb->id);
            print_asm("jmp panic\n");
            break;
        }
        case CFCInstruction::syscall: {
            // TODO: the fact that syscalls have a direct continuation mapping is bad and hinders optimization here
            // should replace with target inputs!!
            // TODO: also, when we have target inputs, we need to recognize calling convention and save registers!
            // TODO: also make sure we let the lifter remove inputs somehow so we spare some registers
            // cause they get clobbered on riscv syscalls as well iirc
            const auto &info = std::get<CfOp::SyscallInfo>(cf_op.info);
            write_static_mapping(info.continuation_block, cur_time, info.continuation_mapping);
            // cur_time += 1 + info.continuation_mapping.size();

            for (size_t i = 0; i < call_reg.size(); ++i) {
                auto *var = cf_op.in_vars[i].get();
                if (var == nullptr)
                    break;

                if (var->type == Type::mt)
                    continue;

                const auto reg = call_reg[i];
                if (reg_map[reg].cur_var && reg_map[reg].cur_var->gen_info.last_use_time >= cur_time) {
                    save_reg(reg);
                }
                load_val_in_reg(cur_time, var, call_reg[i]);
            }
            if (cf_op.in_vars[6] == nullptr) {
                print_asm("sub rsp, 16\n");
            } else {
                // TODO: clear rax before when we have inputs < 64 bit
                if (reg_map[REG_A].cur_var && reg_map[REG_A].cur_var->gen_info.last_use_time >= cur_time) {
                    save_reg(REG_A);
                }
                load_val_in_reg(cur_time, cf_op.in_vars[6].get(), REG_A);
                print_asm("sub rsp, 8\n");
                print_asm("push rax\n");
            }

            print_asm("call syscall_impl\n");
            if (info.static_mapping.size() > 0) {
                print_asm("mov [s%zu], rax\n", info.static_mapping.at(0));
            }
            print_asm("# destroy stack space\n");
            print_asm("add rsp, %zu\n", max_stack_frame_size * 8 + 16);
            // need to jump to translation block
            // TODO: technically we don't need to if the block didn't have a input mapping before
            // so only do that when the next block does have an input mapping or more than one predecessor?
            print_asm("jmp b%zu\n", info.continuation_block->id);
            break;
        }
        default: {
            assert(0);
            exit(1);
        }
        }
    }
}

void RegAlloc::write_assembled_blocks(size_t max_stack_frame_size) {
    for (size_t i = 0; i < assembled_blocks.size(); ++i) {
        auto &block = assembled_blocks[i];

        // reset geninfos since it gets overwritten when the block is compiled
        /*for (auto& cf_op : block.bb->control_flow_ops) {
            auto* target = cf_op.target();
            if (!target) {
                continue;
            }
            for (size_t input_idx = 0; input_idx < target->inputs.size(); ++input_idx) {
                auto& info = target->gen_info.input_map[input_idx];
                auto* input = target->inputs[input_idx];
                input->gen_info.saved_in_stack = false;
                if (info.location == BasicBlock::GeneratorInfo::InputInfo::STATIC) {
                    input->gen_info.location = SSAVar::GeneratorInfoX64::STATIC;
                    input->gen_info.static_idx = info.static_idx;
                } else if (info.location == BasicBlock::GeneratorInfo::InputInfo::REGISTER) {
                    input->gen_info.location = SSAVar::GeneratorInfoX64::REGISTER;
                    input->gen_info.reg_idx = info.reg_idx;
                } else if (info.location == BasicBlock::GeneratorInfo::InputInfo::STACK) {
                    input->gen_info.location = SSAVar::GeneratorInfoX64::STACK_FRAME;
                    input->gen_info.saved_in_stack = true;
                    input->gen_info.stack_slot = info.stack_slot;
                }
            }
        }*/

        fprintf(gen->out_fd, "%s\n", block.assembly.c_str());
        asm_buf.clear();
        cur_bb = block.bb;
        cur_reg_map = &block.reg_map;
        cur_stack_map = &block.stack_map;
        compile_cf_ops(block.bb, block.reg_map, block.stack_map, max_stack_frame_size);
        fprintf(gen->out_fd, "%s\n", asm_buf.c_str());
    }
    cur_bb = nullptr;
    cur_reg_map = nullptr;
    cur_stack_map = nullptr;
}

void RegAlloc::generate_translation_block(BasicBlock *bb) {
    std::string tmp_buf = {};
    std::swap(tmp_buf, asm_buf);

    bool rax_input = false;
    size_t rax_static = 0;
    for (size_t i = 0; i < bb->inputs.size(); ++i) {
        auto *var = bb->inputs[i];
        if (var->type == Type::mt) {
            continue;
        }

        const auto &input_info = bb->gen_info.input_map[i];
        // only allow static inputs for now
        assert(std::holds_alternative<size_t>(var->info));

        const auto src_static = std::get<size_t>(var->info);
        switch (input_info.location) {
        case BasicBlock::GeneratorInfo::InputInfo::REGISTER:
            if (input_info.reg_idx == REG_A) {
                rax_input = true;
                rax_static = src_static;
                break;
            }
            print_asm("mov %s, [s%zu]\n", reg_names[input_info.reg_idx][0], src_static);
            break;
        case BasicBlock::GeneratorInfo::InputInfo::STACK:
            print_asm("mov rax, [s%zu]\n", src_static);
            print_asm("mov [rsp + 8 * %zu], rax\n", input_info.stack_slot);
            break;
        case BasicBlock::GeneratorInfo::InputInfo::STATIC:
            if (input_info.static_idx != src_static) {
                // TODO: this can break when two statics are swapped
                print_asm("mov rax, [s%zu]\n", src_static);
                print_asm("mov [s%zu], rax\n", input_info.static_idx);
            }
            break;
        default:
            // other cases shouldn't happen
            assert(0);
            exit(1);
        }
    }

    if (rax_input) {
        print_asm("mov rax, [s%zu]\n", rax_static);
    }
    print_asm("jmp b%zu_reg_alloc\n", bb->id);

    std::swap(tmp_buf, asm_buf);
    translation_blocks.push_back(std::make_pair(bb->id, std::move(tmp_buf)));
}

void RegAlloc::set_bb_inputs(BasicBlock *target, const std::vector<RefPtr<SSAVar>> &inputs) {
    const auto cur_time = cur_bb->variables.size();
    // fix for immediate inputs
    for (size_t i = 0; i < inputs.size(); ++i) {
        auto *input_var = inputs[i].get();
        if (input_var->type == Type::mt) {
            continue;
        }

        if (input_var->gen_info.location != SSAVar::GeneratorInfoX64::NOT_CALCULATED) {
            continue;
        }
        assert(input_var->type == Type::imm);
        load_val_in_reg(cur_time, input_var);
    }

    assert(target->inputs.size() == inputs.size());
    if (target->id > BB_MERGE_TIL_ID || is_block_top_level(target)) {
        // cheap fix to force single block register allocation
        set_bb_inputs_from_static(target);
    } else {
        // just write input locations, compile the input map and we done
        for (size_t i = 0; i < inputs.size(); ++i) {
            auto *input_var = inputs[i].get();
            auto *target_var = target->inputs[i];
            if (input_var->type == Type::mt) {
                continue;
            }

            assert(input_var->gen_info.location != SSAVar::GeneratorInfoX64::NOT_CALCULATED);
            target_var->gen_info.location = input_var->gen_info.location;
            target_var->gen_info.loc_info = input_var->gen_info.loc_info;

            // TODO: make translation blocks/cfops put the values in the registers/statics *and* stack locations
            // if applicable
            if (input_var->gen_info.location == SSAVar::GeneratorInfoX64::STACK_FRAME) {
                target_var->gen_info.saved_in_stack = true;
                target_var->gen_info.stack_slot = input_var->gen_info.stack_slot;
            }
        }

        generate_input_map(target);
    }
}

void RegAlloc::set_bb_inputs_from_static(BasicBlock *target) {
    for (size_t i = 0; i < target->inputs.size(); ++i) {
        auto *var = target->inputs[i];
        assert(std::holds_alternative<size_t>(var->info));

        BasicBlock::GeneratorInfo::InputInfo info;
        info.location = BasicBlock::GeneratorInfo::InputInfo::STATIC;
        info.static_idx = std::get<size_t>(var->info);
        var->gen_info.location = SSAVar::GeneratorInfoX64::STATIC;
        var->gen_info.static_idx = info.static_idx;
        target->gen_info.input_map.push_back(std::move(info));
    }
    target->gen_info.input_map_setup = true;
}

void RegAlloc::generate_input_map(BasicBlock *bb) {
    for (size_t i = 0; i < bb->inputs.size(); ++i) {
        // namespaces :D
        using InputInfo = BasicBlock::GeneratorInfo::InputInfo;
        using GenInfo = SSAVar::GeneratorInfoX64;

        auto *var = bb->inputs[i];
        if (var->type == Type::mt) {
            // we lie a little
            InputInfo info;
            info.location = InputInfo::STATIC;
            info.static_idx = i;
            bb->gen_info.input_map.push_back(std::move(info));
            continue;
        }

        InputInfo info;
        const auto var_loc = var->gen_info.location;
        assert(var_loc == GenInfo::REGISTER || var_loc == GenInfo::STACK_FRAME || var_loc == GenInfo::STATIC);
        if (var_loc == GenInfo::STATIC) {
            info.location = InputInfo::STATIC;
            info.static_idx = var->gen_info.static_idx;
        } else if (var_loc == GenInfo::REGISTER) {
            info.location = InputInfo::REGISTER;
            info.reg_idx = var->gen_info.reg_idx;
        } else if (var_loc == GenInfo::STACK_FRAME) {
            info.location = InputInfo::STACK;
            info.stack_slot = var->gen_info.stack_slot;
        }
        bb->gen_info.input_map.push_back(std::move(info));
    }

    bb->gen_info.input_map_setup = true;
}

void RegAlloc::write_static_mapping([[maybe_unused]] BasicBlock *bb, size_t cur_time, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping) {
    // TODO: this is a bit unfaithful to the time calculation since we write out registers first but that should be fine

    // TODO: here we could write out all register that do not overwrite a static that is uses as an input
    // but that involves a bit more housekeeping
    // load all static inputs into a register so we don't have to worry about that anymore
    for (const auto &pair : mapping) {
        auto *var = pair.first.get();
        if (var->type == Type::mt) {
            continue;
        }
        if (var->gen_info.location != SSAVar::GeneratorInfoX64::STATIC) {
            continue;
        }

        // skip identity-mapped statics
        if (var->gen_info.static_idx == pair.second) {
            continue;
        }

        load_val_in_reg(cur_time, var);
    }

    // write out all registers
    for (const auto &pair : mapping) {
        auto *var = pair.first.get();
        if (var->type == Type::mt) {
            continue;
        }
        if (var->gen_info.location != SSAVar::GeneratorInfoX64::REGISTER) {
            continue;
        }

        print_asm("mov [s%zu], %s\n", pair.second, reg_names[var->gen_info.reg_idx][0]);
    }

    // write out stuff in stack
    for (size_t var_idx = 0; var_idx < mapping.size(); ++var_idx) {
        auto *var = mapping[var_idx].first.get();
        if (var->type == Type::mt) {
            continue;
        }
        const auto static_idx = mapping[var_idx].second;

        if (var->gen_info.location == SSAVar::GeneratorInfoX64::REGISTER || var->gen_info.location == SSAVar::GeneratorInfoX64::STATIC) {
            continue;
        }

        // TODO: cant do that here since the syscall cfop needs some vars later on
        // TODO: really need to fix this time management
        const auto reg = load_val_in_reg(cur_time /*+ var_idx*/, var);
        print_asm("mov [s%zu], %s\n", static_idx, reg_names[reg][0]);
    }
}

void RegAlloc::write_target_inputs(BasicBlock *target, size_t cur_time, const std::vector<RefPtr<SSAVar>> &inputs) {
    // Here we have multiple problems:
    // - we could need to write to a static that is needed to be somewhere else later
    // - we could need to write to a register that is needed somewhere else later
    // - we could need to write to a stack-slot that is needed somewhere else later
    // the dumbest thing to solve these would be to recognize them, force-allocate new stack-slots and load them from there when needed
    // so that's what we do :)
    // register conflicts should be resolved by load_from_reg though

    assert(target->gen_info.input_map_setup);
    assert(target->inputs.size() == target->gen_info.input_map.size());
    const auto &input_map = target->gen_info.input_map;
    // mark all stack slots used as inputs as non-free
    auto &stack_map = *cur_stack_map;
    for (size_t i = 0; i < input_map.size(); ++i) {
        if (input_map[i].location != BasicBlock::GeneratorInfo::InputInfo::STACK) {
            continue;
        }

        const auto stack_slot = input_map[i].stack_slot;
        if (stack_map.size() <= stack_slot) {
            stack_map.resize(stack_slot + 1);
        }
        stack_map[stack_slot].free = false;
    }

    // figure out static conflicts
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (inputs[i]->gen_info.location != SSAVar::GeneratorInfoX64::STATIC) {
            continue;
        }
        if (inputs[i]->type == Type::mt) {
            continue;
        }

        // it is a problem when a previous input needs to be at this static
        auto conflict = false;
        for (size_t j = 0; j < i; ++j) {
            if (input_map[j].location == BasicBlock::GeneratorInfo::InputInfo::STATIC && input_map[j].static_idx == inputs[i]->gen_info.static_idx) {
                conflict = true;
                break;
            }
        }

        if (!conflict) {
            continue;
        }

        // just load it in a register
        load_val_in_reg(cur_time, inputs[i].get());
    }

    // stack conflicts
    for (size_t i = 0; i < inputs.size(); ++i) {
        auto *var = inputs[i].get();
        if (!var->gen_info.saved_in_stack) {
            continue;
        }

        // when two stack slots overlap and they don't correspond to the same stack slot
        auto conflict = false;
        for (size_t j = 0; j < inputs.size(); ++j) {
            if (i == j || input_map[j].location != BasicBlock::GeneratorInfo::InputInfo::STACK) {
                continue;
            }

            if (var->gen_info.stack_slot == input_map[j].stack_slot) {
                conflict = true;
                break;
            }
        }
        if (!conflict) {
            continue;
        }

        // load in register, delete stack slot and save again
        const auto reg = load_val_in_reg(cur_time, var);
        var->gen_info.saved_in_stack = false;
        save_reg(reg);
    }

    // write out statics
    for (size_t var_idx = 0; var_idx < inputs.size(); ++var_idx) {
        if (input_map[var_idx].location != BasicBlock::GeneratorInfo::InputInfo::STATIC) {
            continue;
        }
        if (inputs[var_idx]->type == Type::mt) {
            continue;
        }
        if (inputs[var_idx]->gen_info.location == SSAVar::GeneratorInfoX64::STATIC && inputs[var_idx]->gen_info.static_idx == input_map[var_idx].static_idx) {
            continue;
        }

        const auto reg = load_val_in_reg(cur_time, inputs[var_idx].get());
        print_asm("mov [s%zu], %s\n", input_map[var_idx].static_idx, reg_names[reg][0]);
    }

    // write out stack
    for (size_t var_idx = 0; var_idx < inputs.size(); ++var_idx) {
        auto &info = input_map[var_idx];
        if (info.location != BasicBlock::GeneratorInfo::InputInfo::STACK) {
            continue;
        }
        auto *input = inputs[var_idx].get();
        if (input->gen_info.saved_in_stack && input->gen_info.stack_slot == info.stack_slot) {
            continue;
        }

        const auto reg = load_val_in_reg(cur_time, input);
        print_asm("mov [rsp + 8 * %zu], %s\n", info.stack_slot, reg_names[reg][0]);
    }

    // write out registers
    for (size_t var_idx = 0; var_idx < inputs.size(); ++var_idx) {
        if (input_map[var_idx].location != BasicBlock::GeneratorInfo::InputInfo::REGISTER) {
            continue;
        }

        const auto reg = static_cast<REGISTER>(input_map[var_idx].reg_idx);
        auto *input = inputs[var_idx].get();

        load_val_in_reg(cur_time, input, reg);
    }
}

void RegAlloc::init_time_of_use(BasicBlock *bb) {
    for (size_t i = 0; i < bb->variables.size(); ++i) {
        auto *var = bb->variables[i].get();
        if (!std::holds_alternative<std::unique_ptr<Operation>>(var->info)) {
            continue;
        }

        auto *op = std::get<std::unique_ptr<Operation>>(var->info).get();
        for (auto &input : op->in_vars) {
            if (!input) {
                continue;
            }
            input->gen_info.last_use_time = i; // max(last_use_time, i)?
            input->gen_info.uses.push_back(i);
        }
    }

    const auto set_time_cont_mapping = [](const size_t time_off, std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping) {
        for (size_t i = 0; i < mapping.size(); ++i) {
            auto &info = mapping[i].first->gen_info;
            info.last_use_time = std::max(info.last_use_time, time_off + i);
            info.uses.push_back(time_off + i);
        }
    };

    const auto set_time_inputs = [](const size_t time_off, std::vector<RefPtr<SSAVar>> &mapping) {
        for (size_t i = 0; i < mapping.size(); ++i) {
            auto &info = mapping[i]->gen_info;
            info.last_use_time = std::max(info.last_use_time, time_off + i);
            info.uses.push_back(time_off + i);
        }
    };

    for (auto &cf_op : bb->control_flow_ops) {
        // we treat cf_ops as running in parallel
        auto time_off = bb->variables.size();
        for (auto &input : cf_op.in_vars) {
            if (!input) {
                continue;
            }

            input->gen_info.last_use_time = std::max(input->gen_info.last_use_time, time_off);
            input->gen_info.uses.push_back(time_off);
        }

        time_off++;
        switch (cf_op.type) {
        case CFCInstruction::jump:
            set_time_inputs(time_off, std::get<CfOp::JumpInfo>(cf_op.info).target_inputs);
            break;
        case CFCInstruction::ijump:
            set_time_cont_mapping(time_off, std::get<CfOp::IJumpInfo>(cf_op.info).mapping);
            break;
        case CFCInstruction::cjump:
            set_time_inputs(time_off, std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs);
            break;
        case CFCInstruction::call: {
            auto &info = std::get<CfOp::CallInfo>(cf_op.info);
            set_time_inputs(time_off, info.target_inputs);
            time_off += info.target_inputs.size();
            set_time_cont_mapping(time_off, info.continuation_mapping);
            break;
        }
        case CFCInstruction::icall: {
            auto &info = std::get<CfOp::ICallInfo>(cf_op.info);
            set_time_cont_mapping(time_off, info.mapping);
            time_off += info.mapping.size();
            set_time_cont_mapping(time_off, info.continuation_mapping);
            break;
        }
        case CFCInstruction::_return:
            set_time_cont_mapping(time_off, std::get<CfOp::RetInfo>(cf_op.info).mapping);
            break;
        case CFCInstruction::unreachable:
            break;
        case CFCInstruction::syscall:
            set_time_cont_mapping(time_off, std::get<CfOp::SyscallInfo>(cf_op.info).continuation_mapping);
            break;
        }
    }
}

template <typename... Args> REGISTER RegAlloc::alloc_reg(size_t cur_time, REGISTER only_this_reg, Args... clear_regs) {
    static_assert((std::is_same_v<Args, REGISTER> && ...));
    auto &reg_map = *cur_reg_map;

    if (only_this_reg != REG_NONE) {
        auto &cur_var = reg_map[only_this_reg].cur_var;
        if (cur_var != nullptr) {
            save_reg(only_this_reg);
            cur_var->gen_info.location = SSAVar::GeneratorInfoX64::STACK_FRAME;
            cur_var = nullptr;
        }
        return only_this_reg;
    }

    REGISTER reg = REG_NONE;
    // try to find free register
    for (size_t i = 0; i < REG_COUNT; ++i) {
        if (((i == clear_regs) || ...)) {
            continue;
        }

        if (reg_map[i].cur_var == nullptr) {
            reg = static_cast<REGISTER>(i);
            break;
        }
    }

    if (reg == REG_NONE) {
        // try to find reg with unused var
        for (size_t i = 0; i < REG_COUNT; ++i) {
            if (((i == clear_regs) || ...)) {
                continue;
            }

            if (reg_map[i].cur_var->gen_info.last_use_time < cur_time) {
                reg = static_cast<REGISTER>(i);
                break;
            }
        }

        if (reg == REG_NONE) {
            // find var that's the farthest from being used again
            // TODO: prefer variables that are used less often so that the stack ptr for example stays in a register
            size_t farthest_use_time = 0;
            REGISTER farthest_use_reg = REG_NONE;
            for (size_t i = 0; i < REG_COUNT; ++i) {
                if (((i == clear_regs) || ...)) {
                    continue;
                }

                size_t next_use = 0;
                for (const auto use_time : reg_map[i].cur_var->gen_info.uses) {
                    if (use_time > cur_time) {
                        next_use = use_time;
                        break;
                    }
                }

                if (next_use == 0) {
                    // var is needed in this step
                    continue;
                }
                if (farthest_use_time < next_use) {
                    farthest_use_time = next_use;
                    farthest_use_reg = static_cast<REGISTER>(i);
                }
            }
            assert(farthest_use_reg != REG_NONE);

            reg = farthest_use_reg;
            save_reg(farthest_use_reg);
        }
    }

    clear_reg(cur_time, reg);
    reg_map[reg].cur_var = nullptr;
    reg_map[reg].alloc_time = cur_time;
    return reg;
}

template <typename... Args> REGISTER RegAlloc::load_val_in_reg(size_t cur_time, SSAVar *var, REGISTER only_this_reg, Args... clear_regs) {
    static_assert((std::is_same_v<Args, REGISTER> && ...));
    auto &reg_map = *cur_reg_map;

    if (var->gen_info.location == SSAVar::GeneratorInfoX64::REGISTER) {
        // TODO: dont ignore clear_regs here
        if (only_this_reg == REG_NONE || var->gen_info.reg_idx == only_this_reg) {
            return static_cast<REGISTER>(var->gen_info.reg_idx);
        }

        // TODO: we could also simply xchg the registers if the one in the other register doesn't need to be in there
        // so save if the reg has been allocated using only_this_reg in the reg_map?
        // TODO: also this will bug out when you alloc a reg and then alloc one if only_this_reg and they end up in the same register
        if (auto *other_var = reg_map[only_this_reg].cur_var; other_var && other_var->gen_info.last_use_time >= cur_time) {
            // assert(reg_map[only_this_reg].cur_var->gen_info.last_use_time != cur_time);

            // save_reg(only_this_reg);

            print_asm("xchg %s, %s\n", reg_names[only_this_reg][0], reg_names[var->gen_info.reg_idx][0]);
            std::swap(reg_map[only_this_reg], reg_map[var->gen_info.reg_idx]);
            std::swap(var->gen_info.reg_idx, other_var->gen_info.reg_idx);
            return only_this_reg;
        }
        clear_reg(cur_time, only_this_reg);
        print_asm("mov %s, %s\n", reg_names[only_this_reg][0], reg_names[var->gen_info.reg_idx][0]);
        reg_map[var->gen_info.reg_idx].cur_var = nullptr;
        reg_map[only_this_reg].cur_var = var;
        var->gen_info.reg_idx = only_this_reg;
        return only_this_reg;
    }

    const auto reg = alloc_reg(cur_time, only_this_reg, clear_regs...);

    if (var->type == Type::imm) {
        auto &info = std::get<SSAVar::ImmInfo>(var->info);
        if (info.binary_relative) {
            print_asm("lea %s, [binary + %ld]\n", reg_names[reg][0], info.val);
        } else {
            print_asm("mov %s, %ld\n", reg_names[reg][0], info.val);
        }
    } else {
        // non-immediates should have been calculated before
        assert(var->gen_info.location != SSAVar::GeneratorInfoX64::NOT_CALCULATED);
        if (var->gen_info.location == SSAVar::GeneratorInfoX64::STATIC) {
            print_asm("mov %s, [s%zu]\n", reg_name(reg, var->type), var->gen_info.static_idx);
        } else {
            print_asm("mov %s, [rsp + 8 * %zu]\n", reg_name(reg, var->type), var->gen_info.stack_slot);
        }
    }

    reg_map[reg].cur_var = var;
    var->gen_info.location = SSAVar::GeneratorInfoX64::REGISTER;
    var->gen_info.reg_idx = reg;
    return reg;
}

void RegAlloc::clear_reg(size_t cur_time, REGISTER reg) {
    auto &reg_map = *cur_reg_map;
    auto *var = reg_map[reg].cur_var;
    if (!var) {
        return;
    }

    if (var->type == Type::imm) {
        // we simply calculate the value on demand
        var->gen_info.location = SSAVar::GeneratorInfoX64::NOT_CALCULATED;
    } else if (var->gen_info.saved_in_stack) {
        var->gen_info.location = SSAVar::GeneratorInfoX64::STACK_FRAME;
    } else {
        // var that was never saved on stack and is not needed anymore
        // TODO: <?
        assert(var->gen_info.last_use_time <= cur_time);
        var->gen_info.location = SSAVar::GeneratorInfoX64::NOT_CALCULATED;
    }
    reg_map[reg].cur_var = nullptr;
}

void RegAlloc::save_reg(REGISTER reg) {
    auto &reg_map = *cur_reg_map;
    auto &stack_map = *cur_stack_map;

    auto *var = reg_map[reg].cur_var;
    if (!var) {
        return;
    }

    if (var->gen_info.saved_in_stack) {
        // var was already saved, no need to save it again
        return;
    }

    if (var->type == Type::imm) {
        // no need to save immediates i think
        return;
    }

    // find slot for var
    size_t stack_slot = 0;
    {
        auto stack_slot_found = false;
        for (size_t i = 0; i < stack_map.size(); ++i) {
            if (stack_map[i].free) {
                stack_slot_found = true;
                stack_slot = i;
                break;
            }
        }
        if (!stack_slot_found) {
            stack_slot = stack_map.size();
            stack_map.emplace_back();
        }
    }

    print_asm("mov [rsp + 8 * %zu], %s\n", stack_slot, reg_name(reg, var->type));
    stack_map[stack_slot].free = false;
    stack_map[stack_slot].var = var;
    var->gen_info.saved_in_stack = true;
    var->gen_info.stack_slot = stack_slot;
}

void RegAlloc::clear_after_alloc_time(size_t alloc_time) {
    // TODO: doesnt work
    auto &reg_map = *cur_reg_map;
    for (size_t i = 0; i < REG_COUNT; ++i) {
        if (reg_map[i].alloc_time < alloc_time) {
            continue;
        }

        auto *var = reg_map[i].cur_var;
        if (!var) {
            return;
        }

        if (var->type == Type::imm) {
            // we simply calculate the value on demand
            var->gen_info.location = SSAVar::GeneratorInfoX64::NOT_CALCULATED;
        } else if (var->gen_info.saved_in_stack) {
            var->gen_info.location = SSAVar::GeneratorInfoX64::STACK_FRAME;
        } else {
            var->gen_info.location = SSAVar::GeneratorInfoX64::NOT_CALCULATED;
        }
        reg_map[i].cur_var = nullptr;
    }
}

bool RegAlloc::is_block_top_level(BasicBlock *bb) {
    if (bb->id > BB_OLD_COMPILE_ID_TIL || bb->id > BB_MERGE_TIL_ID || bb->gen_info.manual_top_level) {
        return true;
    }

    for (auto *pred : bb->predecessors) {
        if (pred != bb) {
            return false;
        }
    }

    return true;
}