#pragma once

#include "generator/x86_64/assembler.h"
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
    std::string binary_filepath, binary_out;
    FILE *out_fd;
    uint32_t optimizations = 0;
    bool needs_short_jmp_resolve = false;
    Assembler as;

    Generator(IR *ir, std::string binary_filepath = {}, FILE *out_fd = stdout, std::string binary_out = {});

    void compile();

  protected:
    void compile_blocks();
    void compile_block(const BasicBlock *block);

    void compile_ijump(const BasicBlock *block, const CfOp &op);
    void compile_vars(const BasicBlock *block);
    void compile_cf_args(const BasicBlock *block, const CfOp &op);
    void compile_ret_args(const BasicBlock *block, const CfOp &op);
    void compile_cjump(const BasicBlock *block, const CfOp &op, size_t cond_idx);
    void compile_syscall(const BasicBlock *block, const CfOp &op);
    void compile_continuation_args(const BasicBlock *block, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping);
};
} // namespace generator::x86_64
