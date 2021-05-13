#include <iostream>

#include <generator/generator.h>
#include <ir/ir.h>
#include <lifter/lifter.h>

int main()
{
    IR ir       = IR{};
    auto *block = ir.add_basic_block();
    {
        const auto static0 = ir.add_static("r0", Type::i64);
        const auto static1 = ir.add_static("r1", Type::i64);

        auto *in1 = block->add_input(block->add_var_from_static(static0));
        auto *in2 = block->add_input(block->add_var_from_static(static1));

        auto *var1 = block->add_var(Type::i64);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->add_inputs(in1, in2);
            op->add_outputs(var1);
            var1->set_op(std::move(op));
        }

        auto *imm1 = block->add_var_imm(123);
        auto *imm2 = block->add_var_imm(1);
        auto *var2 = block->add_var(Type::i64);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->add_inputs(imm1, imm2);
            op->add_outputs(var2);
            var2->set_op(std::move(op));
        }

        auto *var3 = block->add_var(Type::i64);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->add_inputs(var1, var2);
            op->add_outputs(var3);
            var3->set_op(std::move(op));
        }
    }

    auto *block2 = ir.add_basic_block();
    {
        {
            auto &op = block->add_cf_op(CFCInstruction::cjump, block2);
            op.info  = CfOp::CJumpInfo{CfOp::CJumpInfo::CJumpType::eq};

            block->add_cf_op(CFCInstruction::jump, block2);
        }
    }

    ir.print(std::cout);

    return 0;
}
