#pragma once

#include <lifter/program.h>
#include <ir/ir.h>

namespace lifter::RV64 {
    class Lifter {
    public:
        IR *ir;

        explicit Lifter(IR *ir) : ir(ir) {}

        void lift(Program *);

    private:

        size_t next_bb_id{0};
        size_t next_var_id{0};

        void parse_instruction(RV64Inst, Function *, BasicBlock *);

        [[nodiscard]] BasicBlock *get_bb(uint64_t addr) const;

        void liftRec(Program *prog, uint64_t start_addr);

        void liftInvalid(Function *, BasicBlock *);

        void liftAdd(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftAddW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftAddI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftAddIW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSLLI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSLLIW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSLTI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSLTIU(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftXORI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSRAI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSRAIW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSRLI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSRLIW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftORI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftANDI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSLL(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSLLW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSLT(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSLTU(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftXOR(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSRL(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSRLW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftOR(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftAND(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSUB(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSUBW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSRA(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftSRAW(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftFENCE(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftFENCEI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftAUIPC(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftLUI(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftJAL(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftJALR(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftBranch(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftECALL(Function *, BasicBlock *, std::array<SSAVar *, 32>);

        void liftLoad(Function *, BasicBlock *, std::array<SSAVar *, 32>, SSAVar *memory_token);

        void liftStore(Function *, BasicBlock *, std::array<SSAVar *, 32>, SSAVar *memory_token);
    };
}

/*
 * mov x0, 1        // v0 = immediate (1) -> set_last_register_value(x0, v0)
 * mov x1, 2        // v1 = immediate (2) -> set_last_register_value(x1, v1)
 * add x2, x0, x1   // v2 = add get_last_register_value(x0), get_last_register_value(x1) -> set_last_register_value(x2, v2)
 * mov x3, x2
 */
