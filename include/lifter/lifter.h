#pragma once

#include <ir/ir.h>
#include <lifter/program.h>

// Index of memory token in <reg_map> register mapping
#define MEM_IDX 32

namespace lifter::RV64 {
class Lifter {
  public:
    IR *ir;

    explicit Lifter(IR *ir) : ir(ir), dummy() {}

    void lift(Program *);

  private:
    // {0}: not used
    // {1, ..., 31}: RV-Registers
    // {32}: the last valid memory token
    using reg_map = std::array<SSAVar *, 33>;

    // currently used for unresolved jumps
    BasicBlock *dummy;

    static void parse_instruction(RV64Inst instr, BasicBlock *bb, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    [[nodiscard]] BasicBlock *get_bb(uint64_t addr) const;

    void liftRec(Program *prog, Function *func, uint64_t start_addr, BasicBlock *curr_bb);

    static void liftInvalid(BasicBlock *bb, uint64_t ip);

    static void lift_shift(BasicBlock *, RV64Inst &, reg_map &, const Instruction &, const Type &);

    static void lift_shift_immediate(BasicBlock *, RV64Inst &, reg_map &, const Instruction &, const Type &);

    //        void liftSLTI(BasicBlock *, RV64Inst &, reg_map &);
    //
    //        void liftSLTIU(BasicBlock *, RV64Inst &, reg_map &);
    //
    //        void liftSLT(BasicBlock *, RV64Inst &, reg_map &);
    //
    //        void liftSLTU(BasicBlock *, RV64Inst &, reg_map &);
    //
    //        void liftFENCE(BasicBlock *, reg_map&);
    //
    //        void liftFENCEI(BasicBlock *, reg_map&);
    //
    static void liftAUIPC(BasicBlock *, RV64Inst &instr, reg_map &, uint64_t ip);

    static void liftLUI(BasicBlock *, RV64Inst &instr, reg_map &);

    static void liftJAL(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    static void liftJALR(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t next_addr);

    static void liftBranch(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    static void liftECALL(BasicBlock *bb, uint64_t next_addr);

    static void lift_load(BasicBlock *, RV64Inst &, reg_map &, const Type &, bool);

    static void lift_store(BasicBlock *, RV64Inst &, reg_map &, const Type &);

    static void lift_arithmetical_logical(BasicBlock *, RV64Inst &, reg_map &, const Instruction &, const Type &);

    static void lift_arithmetical_logical_immediate(BasicBlock *, RV64Inst &, reg_map &, const Instruction &, const Type &);

    // helpers for lifting and code reduction
    static SSAVar *load_immediate(BasicBlock *bb, int64_t imm);
    static SSAVar *load_immediate(BasicBlock *bb, int32_t imm);
    static SSAVar *shrink_var(BasicBlock *, SSAVar *, const Type &);
};
} // namespace lifter::RV64
