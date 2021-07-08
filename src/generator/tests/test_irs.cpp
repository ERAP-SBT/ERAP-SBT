#include "test_irs.h"

void gen_print_ir(IR &ir) {
    (void)ir.add_static(Type::i64);
    const auto static0 = ir.add_static(Type::i64);
    const auto static1 = ir.add_static(Type::i64);
    const auto static2 = ir.add_static(Type::i64);
    const auto static3 = ir.add_static(Type::i64);
    const auto static4 = ir.add_static(Type::i64);
    const auto static5 = ir.add_static(Type::i64);

    ir.setup_bb_addr_vec(10, 200);

    auto *strlen_entry = ir.add_basic_block(10, "strlen_entry");
    {
        auto *strlen_cmp = ir.add_basic_block(20, "strlen_cmp");
        auto *strlen_inc = ir.add_basic_block(30, "strlen_inc");
        auto *strlen_ret = ir.add_basic_block(40, "strlen_ret");
        {
            // entry
            auto *str_ptr = strlen_entry->add_var_from_static(static0);
            auto *count = strlen_entry->add_var_imm(0, 0);
            auto &cf_op = strlen_entry->add_cf_op(CFCInstruction::jump, strlen_cmp);
            cf_op.add_target_input(str_ptr, 0);
            cf_op.add_target_input(count, 0);
        }
        {
            // cmp
            auto *mem_token = strlen_cmp->add_var(Type::mt, 0); // not proper here
            auto *str_ptr = strlen_cmp->add_var_from_static(static0);
            auto *count = strlen_cmp->add_var_from_static(static5);
            auto *null = strlen_cmp->add_var_imm(0, 0);
            auto *cur_c = strlen_cmp->add_var(Type::i8, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::load);
                op->set_inputs(str_ptr, mem_token);
                op->set_outputs(cur_c);
                cur_c->set_op(std::move(op));
            }

            {
                auto &cf_op = strlen_cmp->add_cf_op(CFCInstruction::cjump, strlen_ret);
                cf_op.set_inputs(cur_c, null);
                std::get<CfOp::CJumpInfo>(cf_op.info).type = CfOp::CJumpInfo::CJumpType::eq;
                std::get<CfOp::CJumpInfo>(cf_op.info).target_inputs.emplace_back(count);
            }
            {
                auto &cf_op = strlen_cmp->add_cf_op(CFCInstruction::jump, strlen_inc);
                std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(str_ptr);
                std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(count);
            }
        }
        {
            // inc
            auto *str_ptr = strlen_inc->add_var_from_static(static0);
            auto *count = strlen_inc->add_var_from_static(static5);
            auto *one = strlen_inc->add_var_imm(1, 0);
            auto *new_str_ptr = strlen_inc->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::add);
                op->set_inputs(str_ptr, one);
                op->set_outputs(new_str_ptr);
                new_str_ptr->set_op(std::move(op));
            }
            auto *new_count = strlen_inc->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::add);
                op->set_inputs(count, one);
                op->set_outputs(new_count);
                new_count->set_op(std::move(op));
            }
            auto &cf_op = strlen_inc->add_cf_op(CFCInstruction::jump, strlen_cmp);
            std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(new_str_ptr);
            std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(new_count);
        }
        {
            auto *count = strlen_ret->add_var_from_static(static0);
            auto &cf_op = strlen_ret->add_cf_op(CFCInstruction::_return, nullptr);
            std::get<CfOp::RetInfo>(cf_op.info).mapping.emplace_back(count, static0);
        }
    }

    auto *entry_block = ir.add_basic_block(50, "entry_block");
    {
        auto *entry_cmp = ir.add_basic_block(60, "entry_cmp");
        auto *entry_exit = ir.add_basic_block(70, "entry_exit");
        auto *entry_strlen = ir.add_basic_block(80, "entry_strlen");
        auto *entry_write = ir.add_basic_block(90, "entry_write");
        auto *entry_write2 = ir.add_basic_block(100, "entry_write2");
        auto *entry_inc = ir.add_basic_block(110, "entry_inc");
        {
            // entry block
            auto *mem_token = entry_block->add_var(Type::mt, 0);
            auto *stack_ptr = entry_block->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::setup_stack);
                op->set_outputs(stack_ptr);
                stack_ptr->set_op(std::move(op));
            }
            auto *argc = entry_block->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::load);
                op->set_inputs(stack_ptr, mem_token);
                op->set_outputs(argc);
                argc->set_op(std::move(op));
            }
            auto *eight = entry_block->add_var_imm(8, 0);
            auto *argv = entry_block->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::add);
                op->set_inputs(stack_ptr, eight);
                op->set_outputs(argv);
                argv->set_op(std::move(op));
            }
            auto &cf_op = entry_block->add_cf_op(CFCInstruction::jump, entry_cmp);
            std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(stack_ptr);
            std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(argc);
            std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(argv);
        }
        {
            // cmp
            auto *stack_ptr = entry_cmp->add_var_from_static(static1);
            auto *argc = entry_cmp->add_var_from_static(static2);
            auto *argv = entry_cmp->add_var_from_static(static3);
            auto *null = entry_cmp->add_var_imm(0, 0);
            {
                auto &cf_op = entry_cmp->add_cf_op(CFCInstruction::cjump, entry_exit);
                std::get<CfOp::CJumpInfo>(cf_op.info).type = CfOp::CJumpInfo::CJumpType::eq;
                cf_op.set_inputs(argc, null);
            }
            {
                auto &cf_op = entry_cmp->add_cf_op(CFCInstruction::jump, entry_strlen);
                std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(stack_ptr);
                std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(argc);
                std::get<CfOp::JumpInfo>(cf_op.info).target_inputs.emplace_back(argv);
            }
        }
        {
            // exit
            auto *v1 = entry_exit->add_var_imm(93, 0);
            auto *v2 = entry_exit->add_var_imm(0, 0);
            auto &cf_op = entry_exit->add_cf_op(CFCInstruction::syscall, entry_exit);
            cf_op.set_inputs(v1, v2);
        }
        {
            auto *mem_token = entry_strlen->add_var(Type::mt, 0);
            // strlen (get str ptr, call strlen, then continue to write)
            auto *stack_ptr = entry_strlen->add_var_from_static(static1);
            auto *argc = entry_strlen->add_var_from_static(static2);
            auto *argv = entry_strlen->add_var_from_static(static3);
            auto *str_ptr = entry_strlen->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::load);
                op->set_inputs(argv, mem_token);
                op->set_outputs(str_ptr);
                str_ptr->set_op(std::move(op));
            }
            auto &cf_op = entry_strlen->add_cf_op(CFCInstruction::call, strlen_entry);
            auto &info = std::get<CfOp::CallInfo>(cf_op.info);
            info.target_inputs.emplace_back(str_ptr);
            info.continuation_mapping.emplace_back(stack_ptr, static1);
            info.continuation_mapping.emplace_back(argc, static2);
            info.continuation_mapping.emplace_back(argv, static3);
            info.continuation_mapping.emplace_back(str_ptr, static4);
            info.continuation_block = entry_write;
        }
        {
            // write 1
            auto *strlen = entry_write->add_var_from_static(static0);
            auto *stack_ptr = entry_write->add_var_from_static(static1);
            auto *argc = entry_write->add_var_from_static(static2);
            auto *argv = entry_write->add_var_from_static(static3);
            auto *str_ptr = entry_write->add_var_from_static(static4);

            auto *id = entry_write->add_var_imm(64, 0); // write
            auto *fd = entry_write->add_var_imm(1, 0);  // stdout

            auto &cf_op = entry_write->add_cf_op(CFCInstruction::syscall, entry_write2);
            cf_op.set_inputs(id, fd, str_ptr, strlen);
            auto &info = std::get<CfOp::SyscallInfo>(cf_op.info);
            info.continuation_mapping.emplace_back(stack_ptr, static1);
            info.continuation_mapping.emplace_back(argc, static2);
            info.continuation_mapping.emplace_back(argv, static3);
        }
        {
            // write 2
            auto *stack_ptr = entry_write2->add_var_from_static(static1);
            auto *argc = entry_write2->add_var_from_static(static2);
            auto *argv = entry_write2->add_var_from_static(static3);

            auto *newline = entry_write2->add_var_imm(0xA, 0);
            auto *eight = entry_write2->add_var_imm(8, 0);
            auto *new_stack_ptr = entry_write2->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::sub);
                op->set_inputs(stack_ptr, eight);
                op->set_outputs(new_stack_ptr);
                new_stack_ptr->set_op(std::move(op));
            }
            auto *mt = entry_write2->add_var(Type::mt, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::store);
                op->set_inputs(new_stack_ptr, newline);
                op->set_outputs(mt);
                mt->set_op(std::move(op));
            }

            auto *id = entry_write2->add_var_imm(64, 0); // write
            auto *fd = entry_write2->add_var_imm(1, 0);  // stdout
            auto *len = entry_write2->add_var_imm(1, 0);

            auto &cf_op = entry_write2->add_cf_op(CFCInstruction::syscall, entry_inc);
            cf_op.set_inputs(id, fd, new_stack_ptr, len);
            auto &info = std::get<CfOp::SyscallInfo>(cf_op.info);
            info.continuation_mapping.emplace_back(stack_ptr, static1);
            info.continuation_mapping.emplace_back(argc, static2);
            info.continuation_mapping.emplace_back(argv, static3);
        }
        {
            // inc block
            auto *stack_ptr = entry_inc->add_var_from_static(static1);
            auto *argc = entry_inc->add_var_from_static(static2);
            auto *argv = entry_inc->add_var_from_static(static3);

            auto *one = entry_inc->add_var_imm(1, 0);
            auto *new_argc = entry_inc->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::sub);
                op->set_inputs(argc, one);
                op->set_outputs(new_argc);
                new_argc->set_op(std::move(op));
            }

            auto *eight = entry_inc->add_var_imm(8, 0);
            auto *new_argv = entry_inc->add_var(Type::i64, 0);
            {
                auto op = std::make_unique<Operation>(Instruction::add);
                op->set_inputs(argv, eight);
                op->set_outputs(new_argv);
                new_argv->set_op(std::move(op));
            }

            auto &cf_op = entry_inc->add_cf_op(CFCInstruction::jump, entry_cmp);
            auto &info = std::get<CfOp::JumpInfo>(cf_op.info);
            info.target_inputs.emplace_back(stack_ptr);
            info.target_inputs.emplace_back(new_argc);
            info.target_inputs.emplace_back(new_argv);
        }
    }

    ir.entry_block = entry_block->id;
}

void gen_unreachable_ir(IR &ir) {
    (void)ir.add_static(Type::i64);

    ir.setup_bb_addr_vec(10, 100);

    auto *block1 = ir.add_basic_block(10);
    { block1->add_cf_op(CFCInstruction::unreachable, nullptr); }

    ir.entry_block = block1->id;
}

void gen_syscall_ir(IR &ir) {
    (void)ir.add_static(Type::i64);

    ir.setup_bb_addr_vec(10, 100);

    auto *block1 = ir.add_basic_block(10);
    {
        auto *v1 = block1->add_var_imm(93, 0);
        auto *v2 = block1->add_var_imm(50, 0);
        auto &cf_op = block1->add_cf_op(CFCInstruction::syscall, block1);
        cf_op.set_inputs(v1, v2);
    }

    ir.entry_block = block1->id;
}

void gen_third_ir(IR &ir) {
    (void)ir.add_static(Type::i64);
    const auto static0 = ir.add_static(Type::i64);

    ir.setup_bb_addr_vec(10, 100);

    auto *block1 = ir.add_basic_block(10);
    {
        auto *v1 = block1->add_var_imm(2, 0);
        auto *v2 = block1->add_var_imm(3, 0);
        auto *v3 = block1->add_var(Type::i64, 0); // v3 = v1 + v2 = 5
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->set_inputs(v1, v2);
            op->set_outputs(v3);
            v3->set_op(std::move(op));
        }
        auto *v4 = block1->add_var(Type::i64, 0); // v4 = v3 - v1 = 3
        {
            auto op = std::make_unique<Operation>(Instruction::sub);
            op->set_inputs(v3, v1);
            op->set_outputs(v4);
            v4->set_op(std::move(op));
        }
        auto *v5 = block1->add_var(Type::i64, 0); // v5 = v4 * v3 = 15
        {
            auto op = std::make_unique<Operation>(Instruction::mul_l);
            op->set_inputs(v4, v3);
            op->set_outputs(v5);
            v5->set_op(std::move(op));
        }
        auto *v6 = block1->add_var(Type::i64, 0); // v6 = v5 / v2 = 5
        {
            auto op = std::make_unique<Operation>(Instruction::div);
            op->set_inputs(v5, v2);
            op->set_outputs(v6);
            v6->set_op(std::move(op));
        }
        auto *v7 = block1->add_var(Type::i64, 0); // v7 = v6 << v4 = 40
        {
            auto op = std::make_unique<Operation>(Instruction::shl);
            op->set_inputs(v6, v4);
            op->set_outputs(v7);
            v7->set_op(std::move(op));
        }
        auto *v8 = block1->add_var(Type::i64, 0); // v8 = v7 >> v1 = 10
        {
            auto op = std::make_unique<Operation>(Instruction::shr);
            op->set_inputs(v7, v1);
            op->set_outputs(v8);
            v8->set_op(std::move(op));
        }
        auto *v9 = block1->add_var(Type::i64, 0); // v9 = v8 | v7 = 42
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(v8, v7);
            op->set_outputs(v9);
            v9->set_op(std::move(op));
        }
        auto *v10 = block1->add_var(Type::i64, 0); // v10 = v9 & v7 = 40
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->set_inputs(v9, v7);
            op->set_outputs(v10);
            v10->set_op(std::move(op));
        }
        auto *v11 = block1->add_var(Type::i64, 0); // v11 = ~v10 = -41 (0xFFFF_FFFF_FFFF_FFD7)
        {
            auto op = std::make_unique<Operation>(Instruction::_not);
            op->set_inputs(v10);
            op->set_outputs(v11);
            v11->set_op(std::move(op));
        }
        auto *v12 = block1->add_var(Type::i64, 0); // v12 = v11 ^ v9 = -3 (0xFFFF_FFFF_FFFF_FFFD)
        {
            auto op = std::make_unique<Operation>(Instruction::_xor);
            op->set_inputs(v11, v9);
            op->set_outputs(v12);
            v12->set_op(std::move(op));
        }
        auto *v13 = block1->add_var(Type::i64, 0); // v13 = ~v12 = 2
        {
            auto op = std::make_unique<Operation>(Instruction::_not);
            op->set_inputs(v12);
            op->set_outputs(v13);
            v13->set_op(std::move(op));
        }

        {
            auto &cf_op = block1->add_cf_op(CFCInstruction::_return, nullptr);
            std::get<CfOp::RetInfo>(cf_op.info).mapping.emplace_back(v13, static0);
        }
    }

    ir.entry_block = block1->id;
}

void gen_sec_ir(IR &ir) {
    (void)ir.add_static(Type::i64);
    const auto static0 = ir.add_static(Type::i64);

    ir.setup_bb_addr_vec(10, 100);

    auto *block1 = ir.add_basic_block(10);
    {
        auto *v1 = block1->add_var_imm(1, 0);
        auto &op = block1->add_cf_op(CFCInstruction::_return, nullptr);
        std::get<CfOp::RetInfo>(op.info).mapping.emplace_back(v1, static0);
    }

    auto *block2 = ir.add_basic_block(20);
    {
        auto *v1 = block2->add_var_imm(0, 0);
        auto &op = block2->add_cf_op(CFCInstruction::_return, nullptr);
        std::get<CfOp::RetInfo>(op.info).mapping.emplace_back(v1, static0);
    }

    auto *entry_block = ir.add_basic_block(30);
    {
        auto *v1 = entry_block->add_var_from_static(static0);
        auto *v2 = entry_block->add_var_imm(3, 0);

        {
            auto &op = entry_block->add_cf_op(CFCInstruction::cjump, block2);
            std::get<CfOp::CJumpInfo>(op.info).type = CfOp::CJumpInfo::CJumpType::lt;
            op.set_inputs(v1, v2);
        }
        { entry_block->add_cf_op(CFCInstruction::jump, block1); }
    }

    ir.entry_block = entry_block->id;
}

void gen_first_ir(IR &ir) {
    (void)ir.add_static(Type::i64);
    const auto static0 = ir.add_static(Type::i64);
    const auto static1 = ir.add_static(Type::i64);

    ir.setup_bb_addr_vec(10, 100);

    auto *block = ir.add_basic_block(10);
    {
        auto *in1 = block->add_var_from_static(static0);
        auto *in2 = block->add_var_from_static(static1);

        auto *var1 = block->add_var(Type::i64, 0);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->set_inputs(in1, in2);
            op->set_outputs(var1);
            var1->set_op(std::move(op));
        }

        auto *imm1 = block->add_var_imm(123, 0);
        auto *imm2 = block->add_var_imm(1, 0);
        auto *var2 = block->add_var(Type::i64, 0);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->set_inputs(imm1, imm2);
            op->set_outputs(var2);
            var2->set_op(std::move(op));
        }

        auto *var3 = block->add_var(Type::i64, 0);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->set_inputs(var1, var2);
            op->set_outputs(var3);
            var3->set_op(std::move(op));
        }

        {
            auto &op = block->add_cf_op(CFCInstruction::_return, nullptr);
            auto &ret_info = std::get<CfOp::RetInfo>(op.info);
            ret_info.mapping.emplace_back(var3, 0);
        }
    }

    auto *block2 = ir.add_basic_block(20);
    {
        {
            // auto &op = block->add_cf_op(CFCInstruction::cjump, block2);
            // op.info  = CfOp::CJumpInfo{CfOp::CJumpInfo::CJumpType::eq};
            auto *imm1 = block2->add_var_imm(1, 0);
            auto *imm2 = block2->add_var_imm(2, 0);
            auto &op = block2->add_cf_op(CFCInstruction::jump, block);
            op.add_target_input(imm1, 0);
            op.add_target_input(imm2, 0);

            // block->add_cf_op(CFCInstruction::jump, block2);
        }
    }

    ir.entry_block = block2->id;
}
