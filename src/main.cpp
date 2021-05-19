#include "generator/generator.h"
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

int main() {
    error_t err = 0;
    if ((err = test_elf_parsing("../rv64test.o"))) {
        return err;
    }

    IR ir = IR{};
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
            auto &op       = block->add_cf_op(CFCInstruction::_return, nullptr);
            op.info        = CfOp::RetInfo{};
            auto &ret_info = std::get<CfOp::RetInfo>(op.info);
            ret_info.mapping.emplace_back(var3, 0);
        }
    }

    auto *block2 = ir.add_basic_block();
    {
        {
            // auto &op = block->add_cf_op(CFCInstruction::cjump, block2);
            // op.info  = CfOp::CJumpInfo{CfOp::CJumpInfo::CJumpType::eq};
            auto *imm1 = block2->add_var_imm(1);
            auto *imm2 = block2->add_var_imm(2);
            auto &op   = block2->add_cf_op(CFCInstruction::jump, block);
            op.add_target_input(imm1);
            op.add_target_input(imm2);

            // block->add_cf_op(CFCInstruction::jump, block2);
        }
    }

    ir.entry_block = block2->id;
    ir.print(std::cout);

    auto gen = generator::x86_64::Generator{&ir};
    gen.compile();

    return 0;
}
