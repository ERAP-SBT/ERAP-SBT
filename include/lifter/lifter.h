#pragma once

#include <ir/ir.h>
#include <lifter/program.h>

namespace lifter::RV64 {
class Lifter {
  public:
    IR *ir;

    explicit Lifter(IR *ir) : ir(ir) {}

    void lift(Program *);

  private:
    size_t next_bb_id{0};

    // {0}: not used
    // {1, ..., 31}: RV-Registers
    // {32}: the last valid memory token
    using reg_map = std::array<SSAVar *, 33>;

    void parse_instruction(RV64Inst instr, BasicBlock *bb, reg_map &mapping);

    [[nodiscard]] BasicBlock *get_bb(uint64_t addr) const;

    BasicBlock *liftRec(Program *prog, Function *func, uint64_t start_addr, BasicBlock *pred);

    //        void liftInvalid(BasicBlock *);

    void lift_shift(BasicBlock *, RV64Inst &, reg_map &, const Instruction &, const Type &);

    void lift_shift_immediate(BasicBlock *, RV64Inst &, reg_map &, const Instruction &, const Type &);

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
    //        void liftAUIPC(BasicBlock *, reg_map&);
    //
    //        void liftLUI(BasicBlock *, reg_map&);
    //
    //        void liftJAL(BasicBlock *, reg_map&);
    //
    //        void liftJALR(BasicBlock *, reg_map&);
    //
    //        void liftBranch(BasicBlock *, reg_map&);
    //
    //        void liftECALL(BasicBlock *, reg_map&);

    void lift_load(BasicBlock *, RV64Inst &, reg_map &, const Type &, bool);

    void lift_store(BasicBlock *, RV64Inst &, reg_map &, const Type&);

    void lift_arithmetical_logical(BasicBlock *, RV64Inst &, reg_map &, const Instruction &, const Type &);

    void lift_arithmetical_logical_immediate(BasicBlock *, RV64Inst &, reg_map &, const Instruction &, const Type &);

    void extend_reg(size_t, BasicBlock *, reg_map &, bool);

    void shrink_reg(size_t, BasicBlock *, reg_map &, const Type &);

    void offset_adding(BasicBlock *, RV64Inst &, reg_map &);
};
} // namespace lifter::RV64

/*
 * mov x0, 1        // v0 = immediate (1) -> set_last_register_value(x0, v0)
 * mov x1, 2        // v1 = immediate (2) -> set_last_register_value(x1, v1)
 * add x2, x0, x1   // v2 = add get_last_register_value(x0), get_last_register_value(x1) -> set_last_register_value(x2, v2)
 * mov x3, x2
 */
