#pragma once

#include "ir/ir.h"

namespace generator::x86_64 {
struct Generator {
    enum class ErrType { unreachable, unresolved_ijump };

    IR *ir;
    std::vector<std::pair<ErrType, const BasicBlock *>> err_msgs;
    std::string binary_filepath;
    FILE *out_fd;

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
    void compile_vars(const BasicBlock *block);
    void compile_cf_args(const BasicBlock *block, const CfOp &op);
    void compile_ret_args(const BasicBlock *block, const CfOp &op);
    void compile_cjump(const BasicBlock *block, const CfOp &op, size_t cond_idx);
    void compile_syscall(const BasicBlock *block, const CfOp &op);
    void compile_continuation_args(const BasicBlock *block, const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &mapping);
};
} // namespace generator::x86_64
