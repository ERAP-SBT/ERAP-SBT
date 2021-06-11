#include "generator/generator.h"
#include "ir/ir.h"
#include "lifter/lifter.h"
#include "lifter/elf_file.h"

// disassembles the code at the entry symbol
void test_elf_parsing(std::string test_path) {
    ELF64File file{test_path};

    Elf64_Sym sym = file.symbols.at(file.start_symbol());

    std::cout << "Start symbol: " << file.symbol_names.at(file.start_symbol()) << "\n";
    std::cout << "Start addr: 0x" << std::hex << sym.st_value << "\n";
    std::cout << "End addr: 0x" << std::hex << sym.st_value + sym.st_size - 1 << "\n\n";

    std::cout << "Bytes (hex):" << "\n";
    for (auto byte : file.bytes_at_symbol(file.start_symbol())) {
        std::cout << std::hex << (int) byte;
        std::cout << " ";
    }

    std::cout << "\n\nDisassembly: \n";
    auto bytes = file.bytes_at_symbol(sym);
    for (size_t off = 0; off < bytes.size();) {
        FrvInst instr;
        off += frv_decode(bytes.size() - off, &bytes[off], FRV_RV64, &instr);
        char str[64];
        frv_format(&instr, 64, str);
        std::cout << str << "\n";
    }
    std::cout << "\n";
}

int main() {
    test_elf_parsing("<test_path>");

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
    }

    auto *block2 = ir.add_basic_block();
    {
        {
            auto &op = block->add_cf_op(CFCInstruction::cjump, block2);
            op.info = CfOp::CJumpInfo{CfOp::CJumpInfo::CJumpType::eq};

            block->add_cf_op(CFCInstruction::jump, block2);
        }
    }

    ir.print(std::cout);

    return 0;
}
