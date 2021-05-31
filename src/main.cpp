#include "argument_parser.h"
#include "ir/ir.h"

#if 0
#include "lifter/elf_file.h"
#include "lifter/lifter.h"
#include "lifter/program.h"
#endif

#if 0
#include "generator/generator.h"
#include "generator/x86_64/generator.h"
#endif

#include <cstdlib>
#include <iostream>

namespace {
void print_help(bool usage_only);
#if 0
void dump_elf(Program &);
#endif
} // namespace

int main(int argc, const char **argv) {
    // Parse arguments, excluding the first entry (executable name)
    Args args(argv + 1, argv + argc);

    if (args.has_argument("help")) {
        print_help(false);
        return EXIT_SUCCESS;
    }

    if (args.positional.empty()) {
        std::cerr << "Missing input file argument!\n";
        print_help(true);
        return EXIT_FAILURE;
    }

    auto elf_path = args.positional[0];

    std::cout << "Translating file " << elf_path << '\n';

    IR ir;

    std::cerr << "ELF file loading is not yet implemented on this branch\n";
#if 0
    Program program(std::make_unique<ELF64File>(elf_path));

    if (args.has_argument("print-disassembly")) {
        std::cout << "------------------------------------------------------------\n";
        std::cout << "Disassembly:\n";
        dump_elf(program);
        std::cout << "------------------------------------------------------------\n";
    }
#endif

    std::cerr << "Lifting is not yet implemented on this branch\n";
#if 0
    {
        lifter::RV64::Lifter lifter(&ir);
        lifter.lift(&program);
    }
#endif

    if (args.has_argument("print-ir")) {
        std::cout << "------------------------------------------------------------\n";
        std::cout << "IR after Lifting:\n";
        ir.print(std::cout);
        std::cout << "------------------------------------------------------------\n";
    }

    std::cerr << "Optimizer passes are not yet implemented on this branch\n";
#if 0
    optimize_ir(ir);
#endif

    std::cerr << "Code generation is not yet implemented on this branch\n";
#if 0
    {
        generator::x86_64::Generator generator(&ir);
        generator.compile();
    }
#endif

    return EXIT_SUCCESS;
}

namespace {
void print_help(bool usage_only) {
    std::cerr << "usage: translate <file> [args...]\n";
    if (!usage_only) {
        std::cerr << "Possible arguments are:\n";
        std::cerr << "    --help:              Shows this help message\n";
        std::cerr << "    --print-ir:          Prints a textual representation of the IR\n";
        std::cerr << "    --print-disassembly: Prints the disassembled input file\n";
    }
}

#if 0
void dump_elf(Program &prog) {
    ELF64File file = *prog.elf_base;
    Elf64_Sym sym = file.symbols[std::find(file.symbol_names.begin(), file.symbol_names.end(), "__libc_start_main") - file.symbol_names.begin()];

    prog.load_symbol_instrs(&sym);

    std::cout << "Start symbol: " << file.symbol_names.at(file.start_symbol()) << "\n";
    std::cout << "Start addr: 0x" << std::hex << sym.st_value << "\n";
    std::cout << "End addr: 0x" << std::hex << sym.st_value + sym.st_size - 1 << "\n\n";

    std::cout << "Disassembly <main>: \n";
    for (size_t i = 0; i < prog.addrs.size(); i++) {
        char str[64];
        frv_format(&std::get<RV64Inst>(prog.data.at(i)).instr, 64, str);
        std::cout << std::hex << prog.addrs.at(i) << ": \t" << str << "\n";
    }
    std::cout << "\n";
}
#endif
} // namespace
