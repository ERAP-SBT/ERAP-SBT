#include "generator/generator.h"
#include "generator/x86_64/generator.h"
#include "ir/ir.h"
#include "lifter/elf_file.h"
#include "lifter/lifter.h"

// disassembles the code at the entry symbol
error_t test_elf_parsing(const std::string &test_path) {
    ELF64File file{test_path};
    error_t err = 0;
    if ((err = file.parse_elf())) {
        return err;
    }

    size_t start_symbol;
    if (!(start_symbol = file.start_symbol().value_or(0))) {
        std::cerr << "Can't find symbol at elf file entry point address.\n";
        return ENOEXEC;
    }
    Elf64_Sym sym = file.symbols.at(start_symbol);

    std::cout << "Start symbol: " << file.symbol_names.at(start_symbol) << "\n";
    std::cout << "Start addr: 0x" << std::hex << sym.st_value << "\n";
    std::cout << "End addr: 0x" << std::hex << sym.st_value + sym.st_size - 1 << "\n\n";

    std::cout << "Bytes (hex):"
              << "\n";

    auto sym_loc = file.bytes_offset(start_symbol);
    for (size_t i = sym_loc.first; i < sym_loc.first + sym_loc.second; i++) {
        std::cout << std::hex << (uint)file.file_content[i];
        std::cout << " ";
    }

    std::cout << "\n\nDisassembly: \n";
    for (size_t off = sym_loc.first; off < sym_loc.first + sym_loc.second;) {
        FrvInst instr;
        off += frv_decode(sym_loc.second - off, &file.file_content[off], FRV_RV64, &instr);
        char str[64];
        frv_format(&instr, 64, str);
        std::cout << str << "\n";
    }
    std::cout << "\n";
    return 0;
}

namespace {
void gen_third_ir(IR &);
void gen_sec_ir(IR &);
void gen_first_ir(IR &);
} // namespace

int main() {
    error_t err = 0;
    if ((err = test_elf_parsing("../rv64test.o"))) {
        return err;
    }

    IR ir = IR{};
    gen_third_ir(ir);
    ir.print(std::cout);

    auto gen = generator::x86_64::Generator{&ir};
    gen.compile();

    return 0;
}

namespace {
void gen_third_ir(IR &ir) {
    const auto static0 = ir.add_static(Type::i64);

    auto *block1 = ir.add_basic_block();
    {
        auto *v1 = block1->add_var_imm(2);
        auto *v2 = block1->add_var_imm(3);
        auto *v3 = block1->add_var(Type::i64); // v3 = v1 + v2 = 5
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->set_inputs(v1, v2);
            op->set_outputs(v3);
            v3->set_op(std::move(op));
        }
        auto *v4 = block1->add_var(Type::i64); // v4 = v3 - v1 = 3
        {
            auto op = std::make_unique<Operation>(Instruction::sub);
            op->set_inputs(v3, v1);
            op->set_outputs(v4);
            v4->set_op(std::move(op));
        }
        auto *v5 = block1->add_var(Type::i64); // v5 = v4 * v3 = 15
        {
            auto op = std::make_unique<Operation>(Instruction::mul);
            op->set_inputs(v4, v3);
            op->set_outputs(v5);
            v5->set_op(std::move(op));
        }
        auto *v6 = block1->add_var(Type::i64); // v6 = v5 / v2 = 5
        {
            auto op = std::make_unique<Operation>(Instruction::div);
            op->set_inputs(v5, v2);
            op->set_outputs(v6);
            v6->set_op(std::move(op));
        }
        auto *v7 = block1->add_var(Type::i64); // v7 = v6 << v4 = 40
        {
            auto op = std::make_unique<Operation>(Instruction::shl);
            op->set_inputs(v6, v4);
            op->set_outputs(v7);
            v7->set_op(std::move(op));
        }
        auto *v8 = block1->add_var(Type::i64); // v8 = v7 >> v1 = 10
        {
            auto op = std::make_unique<Operation>(Instruction::shr);
            op->set_inputs(v7, v1);
            op->set_outputs(v8);
            v8->set_op(std::move(op));
        }
        auto *v9 = block1->add_var(Type::i64); // v9 = v8 | v7 = 42
        {
            auto op = std::make_unique<Operation>(Instruction::_or);
            op->set_inputs(v8, v7);
            op->set_outputs(v9);
            v9->set_op(std::move(op));
        }
        auto *v10 = block1->add_var(Type::i64); // v10 = v9 & v7 = 40
        {
            auto op = std::make_unique<Operation>(Instruction::_and);
            op->set_inputs(v9, v7);
            op->set_outputs(v10);
            v10->set_op(std::move(op));
        }
        auto *v11 = block1->add_var(Type::i64); // v11 = ~v10 = -41 (0xFFFF_FFFF_FFFF_FFD7)
        {
            auto op = std::make_unique<Operation>(Instruction::_not);
            op->set_inputs(v10);
            op->set_outputs(v11);
            v11->set_op(std::move(op));
        }
        auto *v12 = block1->add_var(Type::i64); // v12 = v11 ^ v9 = -3 (0xFFFF_FFFF_FFFF_FFFD)
        {
            auto op = std::make_unique<Operation>(Instruction::_xor);
            op->set_inputs(v11, v9);
            op->set_outputs(v12);
            v12->set_op(std::move(op));
        }
        auto *v13 = block1->add_var(Type::i64); // v13 = ~v12 = 2
        {
            auto op = std::make_unique<Operation>(Instruction::_not);
            op->set_inputs(v12);
            op->set_outputs(v13);
            v13->set_op(std::move(op));
        }

        {
            auto &cf_op = block1->add_cf_op(CFCInstruction::_return, nullptr);
            cf_op.info = CfOp::RetInfo{};
            std::get<CfOp::RetInfo>(cf_op.info).mapping.emplace_back(v13, static0);
        }
    }

    ir.entry_block = block1->id;
}

void gen_sec_ir(IR &ir) {
    const auto static0 = ir.add_static(Type::i64);

    auto *block1 = ir.add_basic_block();
    {
        auto *v1 = block1->add_var_imm(1);
        auto &op = block1->add_cf_op(CFCInstruction::_return, nullptr);
        op.info = CfOp::RetInfo{};
        std::get<CfOp::RetInfo>(op.info).mapping.emplace_back(v1, static0);
    }

    auto *block2 = ir.add_basic_block();
    {
        auto *v1 = block2->add_var_imm(0);
        auto &op = block2->add_cf_op(CFCInstruction::_return, nullptr);
        op.info = CfOp::RetInfo{};
        std::get<CfOp::RetInfo>(op.info).mapping.emplace_back(v1, static0);
    }

    auto *entry_block = ir.add_basic_block();
    {
        auto *v1 = entry_block->add_input(entry_block->add_var_from_static(static0));
        auto *v2 = entry_block->add_var_imm(3);

        {
            auto &op = entry_block->add_cf_op(CFCInstruction::cjump, block2);
            op.info = CfOp::CJumpInfo{CfOp::CJumpInfo::CJumpType::lt};
            op.set_inputs(v1, v2);
        }
        { auto &op = entry_block->add_cf_op(CFCInstruction::jump, block1); }
    }

    ir.entry_block = entry_block->id;
}

void gen_first_ir(IR &ir) {
    auto *block = ir.add_basic_block();
    {
        const auto static0 = ir.add_static(Type::i64);
        const auto static1 = ir.add_static(Type::i64);

        auto *in1 = block->add_input(block->add_var_from_static(static0));
        auto *in2 = block->add_input(block->add_var_from_static(static1));

        auto *var1 = block->add_var(Type::i64);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->set_inputs(in1, in2);
            op->set_outputs(var1);
            var1->set_op(std::move(op));
        }

        auto *imm1 = block->add_var_imm(123);
        auto *imm2 = block->add_var_imm(1);
        auto *var2 = block->add_var(Type::i64);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->set_inputs(imm1, imm2);
            op->set_outputs(var2);
            var2->set_op(std::move(op));
        }

        auto *var3 = block->add_var(Type::i64);
        {
            auto op = std::make_unique<Operation>(Instruction::add);
            op->set_inputs(var1, var2);
            op->set_outputs(var3);
            var3->set_op(std::move(op));
        }

        {
            auto &op = block->add_cf_op(CFCInstruction::_return, nullptr);
            op.info = CfOp::RetInfo{};
            auto &ret_info = std::get<CfOp::RetInfo>(op.info);
            ret_info.mapping.emplace_back(var3, 0);
            block->add_static_output(var3, 0);
        }
    }

    auto *block2 = ir.add_basic_block();
    {
        {
            // auto &op = block->add_cf_op(CFCInstruction::cjump, block2);
            // op.info  = CfOp::CJumpInfo{CfOp::CJumpInfo::CJumpType::eq};
            auto *imm1 = block2->add_var_imm(1);
            auto *imm2 = block2->add_var_imm(2);
            auto &op = block2->add_cf_op(CFCInstruction::jump, block);
            op.add_target_input(imm1);
            op.add_target_input(imm2);

            // block->add_cf_op(CFCInstruction::jump, block2);
        }
    }

    ir.entry_block = block2->id;
}
} // namespace
