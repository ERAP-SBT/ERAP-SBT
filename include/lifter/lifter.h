#pragma once

#include <ir/ir.h>
#include <lifter/program.h>

namespace lifter::RV64 {

/* maximum number of riscv64 instructions a basicblock can have while lifting */
constexpr size_t BASIC_BLOCK_MAX_INSTRUCTIONS = 10000;

/* amount of static variables: 31 registers (x0-x32) + memory token*/
constexpr size_t COUNT_STATIC_VARS = 33;

class Lifter {
  public:
    IR *ir;

    explicit Lifter(IR *ir) : ir(ir), dummy() {}

    void lift(Program *);

    // Register index for constant zero "register" (RISC-V default: 0)
    static constexpr size_t ZERO_IDX = 0;

    // Index of memory token in <reg_map> register mapping
    static constexpr size_t MEM_IDX = 32;

    // Depth of jump address backtracking
    static constexpr int MAX_ADDRESS_SEARCH_DEPTH = 10;

    // lift all data points from the load program header
    static constexpr bool LIFT_ALL_LOAD = true;

    // {0}: not used
    // {1, ..., 31}: RV-Registers
    // {32}: the last valid memory token
    using reg_map = std::array<SSAVar *, COUNT_STATIC_VARS>;

    // currently used for unresolved jumps
    BasicBlock *dummy;

    static void parse_instruction(RV64Inst instr, BasicBlock *bb, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    [[nodiscard]] BasicBlock *get_bb(uint64_t addr) const;

    void lift_rec(Program *prog, Function *func, uint64_t start_addr, std::optional<size_t> addr_idx, BasicBlock *curr_bb);

    static void lift_invalid(BasicBlock *bb, uint64_t ip);

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

    static void lift_mul(BasicBlock *, RV64Inst &, reg_map &, uint64_t, const Instruction &, const Type &);

    static void lift_div(BasicBlock *, RV64Inst &, reg_map &, uint64_t, bool, bool, const Type &);

    // helpers for lifting and code reduction
    static SSAVar *load_immediate(BasicBlock *bb, int64_t imm, uint64_t ip, bool binary_relative, size_t reg = 0);
    static SSAVar *load_immediate(BasicBlock *bb, int32_t imm, uint64_t ip, bool binary_relative, size_t reg = 0);
    static SSAVar *shrink_var(BasicBlock *, SSAVar *, uint64_t ip, const Type &);
    static std::optional<uint64_t> backtrace_jmp_addr(CfOp *, BasicBlock *);
    static std::optional<int64_t> get_var_value(SSAVar *, BasicBlock *, std::vector<SSAVar *> &);
    static std::optional<SSAVar *> get_last_static_assignment(size_t, BasicBlock *);
    void split_basic_block(BasicBlock *, uint64_t, ELF64File *) const;
    static void load_input_vars(BasicBlock *, Operation *, std::vector<int64_t> &, std::vector<SSAVar *> &);
    static std::optional<SSAVar *> convert_type(BasicBlock *, uint64_t, SSAVar *, Type);
    static void print_invalid_op_size(const Instruction &, RV64Inst &);
    static std::string str_decode_instr(const FrvInst *);
    std::vector<RefPtr<SSAVar>> filter_target_inputs(const std::vector<RefPtr<SSAVar>> &old_target_inputs, reg_map new_mapping, uint64_t split_addr) const;
    std::vector<std::pair<RefPtr<SSAVar>, size_t>> filter_target_inputs(const std::vector<std::pair<RefPtr<SSAVar>, size_t>> &old_target_inputs, reg_map new_mapping, uint64_t split_addr) const;
    static SSAVar *get_from_mapping(BasicBlock *, reg_map &, int, int);
    static void write_to_mapping(reg_map &, SSAVar *, int);

    void postprocess();
};
} // namespace lifter::RV64
