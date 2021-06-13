#pragma once

#include "ir/ir.h"

namespace generator::x86_64 {
struct Generator {
    enum class ErrType { unreachable };

    IR *ir;
    std::vector<std::pair<ErrType, BasicBlock *>> err_msgs;
    std::string binary_filepath;

    Generator(IR *ir, std::string binary_filepath = {}) : ir(ir), binary_filepath(std::move(binary_filepath)) {}

    void compile();

  protected:
    void compile_statics();
    void compile_blocks();
    void compile_block(BasicBlock *);
    void compile_entry();
    void compile_err_msgs();

    void compile_vars(const BasicBlock *);
    void compile_cf_args(const BasicBlock *, const CfOp &);
    void compile_ret_args(BasicBlock *, const CfOp &);
    void compile_cjump(const BasicBlock *, const CfOp &, size_t cond_idx);
    void compile_syscall(const BasicBlock *, const CfOp &);
    void compile_continuation_args(const BasicBlock *, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &);
};
} // namespace generator::x86_64
