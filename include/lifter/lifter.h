#pragma once

#include <ir/ir.h>
#include <lifter/program.h>

// Index of memory token in <reg_map> register mapping
#define MEM_IDX 32
// Depth of jump address backtracking
#define MAX_ADDRESS_SEARCH_DEPTH 100
// lift all data points from the load program header
#define LIFT_ALL_LOAD

namespace lifter::RV64 {
class Lifter {
  public:
    IR *ir;

    explicit Lifter(IR *ir) : ir(ir), dummy() {}

    void lift(Program *);

    // private:
    // {0}: not used
    // {1, ..., 31}: RV-Registers
    // {32}: the last valid memory token
    using reg_map = std::array<SSAVar *, 33>;

    // currently used for unresolved jumps
    BasicBlock *dummy;

    static void parse_instruction(RV64Inst instr, BasicBlock *bb, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    [[nodiscard]] BasicBlock *get_bb(uint64_t addr) const;

    void liftRec(Program *prog, Function *func, uint64_t start_addr, std::optional<size_t> addr_idx, BasicBlock *curr_bb);

    static void liftInvalid(BasicBlock *bb, uint64_t ip);

    static void lift_shift(BasicBlock *, RV64Inst &, reg_map &, uint64_t, const Instruction &, const Type &);

    static void lift_shift_immediate(BasicBlock *, RV64Inst &, reg_map &, uint64_t, const Instruction &, const Type &);

    static void lift_slt(BasicBlock *, RV64Inst &, reg_map &, uint64_t, bool, bool);

    static void lift_fence(BasicBlock *, RV64Inst &, uint64_t);

    static void lift_auipc(BasicBlock *, RV64Inst &instr, reg_map &, uint64_t ip);

    static void lift_lui(BasicBlock *, RV64Inst &instr, reg_map &, uint64_t);

    static void lift_jal(BasicBlock *, RV64Inst &, reg_map &, uint64_t, uint64_t);

    static void lift_jalr(BasicBlock *, RV64Inst &, reg_map &, uint64_t, uint64_t);

    static void lift_branch(BasicBlock *, RV64Inst &, reg_map &, uint64_t, uint64_t);

    static void lift_ecall(BasicBlock *, reg_map &, uint64_t, uint64_t);

    static void lift_load(BasicBlock *, RV64Inst &, reg_map &, uint64_t, const Type &, bool);

    static void lift_store(BasicBlock *bb, RV64Inst &, reg_map &, uint64_t, const Type &);

    static void lift_arithmetical_logical(BasicBlock *, RV64Inst &, reg_map &, uint64_t, const Instruction &, const Type &);

    static void lift_arithmetical_logical_immediate(BasicBlock *, RV64Inst &, reg_map &, uint64_t, const Instruction &, const Type &);

    static void lift_mul_div_rem(BasicBlock *, RV64Inst &, reg_map &, uint64_t, const Instruction &, const Type &);

    // helpers for lifting and code reduction
    static SSAVar *load_immediate(BasicBlock *bb, int64_t imm, uint64_t ip, bool binary_relative, size_t reg = 0);
    static SSAVar *load_immediate(BasicBlock *bb, int32_t imm, uint64_t ip, bool binary_relative, size_t reg = 0);
    static SSAVar *shrink_var(BasicBlock *, SSAVar *, uint64_t ip, const Type &);
    static std::optional<uint64_t> backtrace_jmp_addr(CfOp *, BasicBlock *);
    static std::optional<int64_t> get_var_value(SSAVar *, BasicBlock *, std::vector<SSAVar *> &);
    static std::optional<SSAVar *> get_last_static_assignment(size_t, BasicBlock *);
    void split_basic_block(BasicBlock *, uint64_t) const;
    static void load_input_vars(BasicBlock *, Operation *, std::vector<int64_t> &, std::vector<SSAVar *> &);
    static std::optional<SSAVar *> convert_type(BasicBlock *, uint64_t, SSAVar *, Type);
    static void print_invalid_op_size(const Instruction &, RV64Inst &);
    static std::string str_decode_instr(FrvInst *);
    std::vector<RefPtr<SSAVar>> filter_target_inputs(const std::vector<RefPtr<SSAVar>> &old_target_inputs, reg_map new_mapping, uint64_t split_addr) const;
    std::vector<std::pair<RefPtr<SSAVar>, size_t>> filter_target_inputs(const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &old_target_inputs, reg_map new_mapping, uint64_t split_addr) const;
};
} // namespace lifter::RV64
