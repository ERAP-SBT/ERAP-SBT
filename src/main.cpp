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

int main() {
    error_t err = 0;
    if ((err = test_elf_parsing("../rv64test.o"))) {
        return err;
    }

    return 0;
}
