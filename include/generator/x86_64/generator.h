#pragma once

#include "ir/ir.h"

namespace generator::x86_64 {
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
    uint32_t optimizations = 0;

    Generator(IR *ir, std::string binary_filepath = {}, FILE *out_fd = stdout) : ir(ir), binary_filepath(std::move(binary_filepath)), out_fd(out_fd) {}

    void compile();

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

    void compile_ijump(const BasicBlock *block, const CfOp &op);
    void compile_call(const BasicBlock *block, const CfOp &op);
    void compile_icall(const BasicBlock *block, const CfOp &op);
    void compile_vars(const BasicBlock *block);
    void compile_cf_args(const BasicBlock *block, const CfOp &op);
    void compile_ret_args(const BasicBlock *block, const CfOp &op);
    void compile_cjump(const BasicBlock *block, const CfOp &op, size_t cond_idx);
    void compile_syscall(const BasicBlock *block, const CfOp &op);
    void compile_continuation_args(const BasicBlock *block, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping);
};
} // namespace generator::x86_64
