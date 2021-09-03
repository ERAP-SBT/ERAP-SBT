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

    Generator *gen;
    // pair: bb_id, asm
    std::vector<std::pair<size_t, std::string>> translation_blocks;
    char print_buf[512];
    // need to store produced asm as we don't know the size of the stack frame and need to change it later on
    std::string asm_buf = {};
    RegMap *cur_reg_map = nullptr;
    StackMap *cur_stack_map = nullptr;
    BasicBlock *cur_bb = nullptr;

    RegAlloc(Generator *gen) : gen(gen) {}

    void compile_blocks();
    void compile_block(BasicBlock *bb, bool first_block, size_t &max_stack_frame_size);
    void compile_vars(BasicBlock *bb, RegMap &reg_map, StackMap &stack_map);
    void compile_cf_ops(BasicBlock *bb, RegMap &reg_map, StackMap &stack_map);

    void generate_translation_block(BasicBlock *bb);
    void generate_input_map(BasicBlock *bb);
    void write_static_mapping(BasicBlock *bb, size_t cur_time, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping);
    void write_target_inputs(BasicBlock *target, size_t cur_time, const std::vector<RefPtr<SSAVar>> &inputs);
    void init_time_of_use(BasicBlock *bb);

    template <typename... Args> void print_asm(const char *fmt, Args &&...args) {
        snprintf(print_buf, sizeof(print_buf), fmt, args...);
        asm_buf += print_buf;
        // nocheckin
        // fprintf(gen->out_fd, "%s", print_buf);
    }

    template <typename... Args> REGISTER alloc_reg(size_t cur_time, REGISTER only_this_reg = REG_NONE, Args... clear_regs);

    template <typename... Args> REGISTER load_val_in_reg(size_t cur_time, SSAVar *var, REGISTER only_this_reg = REG_NONE, Args... clear_regs);

    // empty reg and do not save the value
    void clear_reg(size_t cur_time, REGISTER reg);

    void save_reg(REGISTER reg);

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
    bool is_block_top_level(BasicBlock *bb);
};

struct Generator {
    enum class ErrType { unreachable, unresolved_ijump };
    enum Optimization : uint32_t { OPT_UNUSED_STATIC = 1 << 0 };

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

    Generator(IR *ir, std::string binary_filepath = {}, FILE *out_fd = stdout) : ir(ir), binary_filepath(std::move(binary_filepath)), out_fd(out_fd) {}

    void compile();
    void compile_block_reg_alloc(const BasicBlock *block);

  protected:
    enum class Section { DATA, BSS, TEXT, RODATA };
    void compile_section(Section section);

    void compile_statics();
    void compile_phdr_info();
    void compile_blocks();
    void compile_block(const BasicBlock *block);
    void compile_entry();
    void compile_err_msgs();
    void compile_ijump_lookup();

    void compile_ijump(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_call(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_icall(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_vars(const BasicBlock *block);
    void compile_cf_args(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_ret_args(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_cjump(const BasicBlock *block, const CfOp &op, size_t cond_idx, size_t stack_size);
    void compile_syscall(const BasicBlock *block, const CfOp &op, size_t stack_size);
    void compile_continuation_args(const BasicBlock *block, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping);
};
} // namespace generator::x86_64
