#pragma once

#include "ir/ir.h"

namespace generator::x86_64 {

struct Generator;

enum REGISTER : uint32_t {
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

// General TODOs:
// - This needs a sensible way to choose which blocks to compile as a group since otherwise you will end up
// with the program going through a lot of translation blocks during normal execution since we e.g. compile a call as a jump
// and therefore regalloc over that
struct RegAlloc {
    struct RegInfo {
        SSAVar *cur_var = nullptr;
        size_t alloc_time = 0;
    };

    struct StackSlot {
        SSAVar *var = nullptr;
        bool free = true;
    };

    using RegMap = std::array<RegInfo, REG_COUNT>;
    using StackMap = std::vector<StackSlot>;
    // TODO: StaticMap?

    struct AssembledBlock {
        BasicBlock *bb;
        RegMap reg_map;
        StackMap stack_map;
        std::string assembly;
    };

    Generator *gen;
    // pair: bb_id, asm
    std::vector<std::pair<size_t, std::string>> translation_blocks;
    // without cfops
    std::vector<AssembledBlock> assembled_blocks;
    char print_buf[512];
    // need to store produced asm as we don't know the size of the stack frame and need to change it later on
    std::string asm_buf = {};
    RegMap *cur_reg_map = nullptr;
    StackMap *cur_stack_map = nullptr;
    BasicBlock *cur_bb = nullptr;

    RegAlloc(Generator *gen) : gen(gen) {}

    void compile_blocks();
    void compile_block(BasicBlock *bb, bool first_block, size_t &max_stack_frame_size, std::vector<BasicBlock *> &compiled_blocks);
    void compile_vars(BasicBlock *bb);
    bool merge_op_unary(size_t cur_time, size_t var_idx);
    bool merge_op_bin(size_t cur_time, size_t var_idx, REGISTER dst_reg);
    void prepare_cf_ops(BasicBlock *bb);
    void compile_cf_ops(BasicBlock *bb, RegMap &reg_map, StackMap &stack_map, size_t max_stack_frame_size, BasicBlock *next_bb, std::vector<BasicBlock *> &compiled_blocks);
    void write_assembled_blocks(size_t max_stack_frame_size, std::vector<BasicBlock *> &compiled_blocks);

    void generate_translation_block(BasicBlock *bb);
    void generate_input_map(BasicBlock *bb);
    void set_bb_inputs_from_static(BasicBlock *target);
    void set_bb_inputs(BasicBlock *target, const std::vector<RefPtr<SSAVar>> &inputs);
    void write_static_mapping(BasicBlock *bb, size_t cur_time, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping);
    void write_target_inputs(BasicBlock *target, size_t cur_time, const std::vector<RefPtr<SSAVar>> &inputs);
    void init_time_of_use(BasicBlock *bb);

    template <typename... Args> void print_asm(const char *fmt, Args &&...args) {
        // NOLINTNEXTLINE(clang-diagnostic-format-security)
        snprintf(print_buf, sizeof(print_buf), fmt, args...);
        asm_buf += print_buf;
        // nocheckin
        // fprintf(gen->out_fd, "%s", print_buf);
    }

    template <bool evict_imms = true, typename... Args> REGISTER alloc_reg(size_t cur_time, REGISTER only_this_reg = REG_NONE, Args... clear_regs);

    template <bool evict_imms = true, typename... Args> REGISTER load_val_in_reg(size_t cur_time, SSAVar *var, REGISTER only_this_reg = REG_NONE, Args... clear_regs);

    // empty reg and do not save the value
    void clear_reg(size_t cur_time, REGISTER reg, bool imm_to_stack = false);

    size_t allocate_stack_slot(SSAVar *var);
    void save_reg(REGISTER reg, bool imm_to_stack = false);

    void set_var_to_reg(size_t cur_time, SSAVar *var, REGISTER reg) {
        auto &reg_map = *cur_reg_map;
        reg_map[reg].cur_var = var;
        reg_map[reg].alloc_time = cur_time;
        var->gen_info.location = SSAVar::GeneratorInfoX64::REGISTER;
        var->gen_info.reg_idx = reg;
    }

    // doesn't save
    void clear_after_alloc_time(size_t alloc_time);

    // TODO: this thing is only needed because smth in the lifter breaks predecessor/successor lists
    static bool is_block_top_level(BasicBlock *bb);

    static bool is_block_jumpable(BasicBlock *bb) {
        if (is_block_top_level(bb) || bb->gen_info.call_cont_block || bb->gen_info.needs_trans_bb) {
            return true;
        }
        assert(bb->gen_info.input_map_setup);
        for (auto &input : bb->gen_info.input_map) {
            if (input.location != BasicBlock::GeneratorInfo::InputInfo::STATIC) {
                return false;
            }
        }

        return true;
    }
};

struct Generator {
    enum class ErrType { unreachable };
    enum Optimization : uint32_t { OPT_UNUSED_STATIC = 1 << 0, OPT_MBRA = 1 << 1, OPT_MERGE_OP = 1 << 2, OPT_ARCH_BMI2 = 1 << 3, OPT_NO_TRANS_BBS = 1 << 4 };

    // Optimization Warnings:
    // OPT_UNUSED_STATIC:
    // this optimization does not allow the swapping of statics to occur in cfops
    // so e.g. in a syscall continuation mapping s0 is mapped to s1 and the other way around
    // though this should never happen in code lifted from a binary

    IR *ir;
    std::vector<std::pair<ErrType, const BasicBlock *>> err_msgs;
    std::string binary_filepath;
    FILE *out_fd;
    std::unique_ptr<RegAlloc> reg_alloc = nullptr;
    uint32_t optimizations = 0;

    const bool interpreter_only;

    Generator(IR *ir, std::string binary_filepath = {}, FILE *out_fd = stdout, bool interpreter_only = false)
        : ir(ir), binary_filepath(std::move(binary_filepath)), out_fd(out_fd), interpreter_only(interpreter_only) {}

    void compile();
    void compile_block(const BasicBlock *block);

  protected:
    enum class Section { DATA, BSS, TEXT, RODATA };
    void compile_section(Section section);

    void compile_statics();
    void compile_phdr_info();
    void compile_interpreter_only_entry();
    void compile_blocks();
    void compile_entry();
    void compile_err_msgs();
    void compile_ijump_lookup();

    void compile_ijump(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_call(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_icall(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_vars(const BasicBlock *block);
    void compile_rounding_mode(const SSAVar *var);
    void compile_cf_args(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_ret(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_cjump(const BasicBlock *block, const CfOp &op, size_t cond_idx, size_t stack_size);
    void compile_syscall(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_continuation_args(const BasicBlock *block, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping);
};
} // namespace generator::x86_64
