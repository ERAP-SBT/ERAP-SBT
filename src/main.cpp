#include "argument_parser.h"
#include "common/internal.h"
#include "generator/x86_64/generator.h"
#include "ir/ir.h"
#include "lifter/elf_file.h"
#include "lifter/lifter.h"

#include <cstdlib>
#include <iostream>

namespace {
void print_help(bool usage_only);
void dump_elf(const ELF64File *);
} // namespace

int main(int argc, const char **argv) {
    // Parse arguments, excluding the first entry (executable name)
    Args args(argv + 1, argv + argc);

    if (args.has_argument("help")) {
        print_help(false);
        return EXIT_SUCCESS;
    }

    if (args.has_argument("debug")) {
        ENABLE_DEBUG = args.get_value_as_bool("debug");
    }

    if (args.positional.empty()) {
        std::cerr << "Missing input file argument!\n";
        print_help(true);
        return EXIT_FAILURE;
    }

    auto elf_path = args.positional[0];

    std::cout << "Translating file " << elf_path << '\n';

    IR ir;

    std::unique_ptr<ELF64File> elf_file = std::make_unique<ELF64File>(elf_path);
    if (elf_file->parse_elf()) {
        return EXIT_FAILURE;
    }

    if (args.has_argument("dump-elf")) {
        std::cout << "------------------------------------------------------------\n";
        std::cout << "Details of ELF file " << elf_path << ":\n";
        dump_elf(elf_file.get());
        std::cout << "------------------------------------------------------------\n";
    }

    if (args.has_argument("output-binary")) {
        FILE *output;
        std::string output_file{args.get_argument("output-binary")};
        if (!(output = fopen(output_file.c_str(), "w"))) {
            auto error_code = errno;
            std::cerr << "The output file could not be opened: " << std::strerror(error_code) << "\n";
            return EXIT_FAILURE;
        }

        const error_t r = elf_file->write_binary_image(output);

        fclose(output);

        if (r != EXIT_SUCCESS) {
            std::cerr << "Error: " << r << "\n";
            return EXIT_FAILURE;
        }
    }

    FILE *output = stdout;
    if (args.has_argument("output")) {
        std::string output_file(args.get_argument("output"));
        if (!(output = fopen(output_file.c_str(), "w"))) {
            auto error_code = errno;
            std::cerr << "The output file could not be opened: " << std::strerror(error_code) << '\n';
            return EXIT_FAILURE;
        }
    }

    Program prog(std::move(elf_file));

    auto lifter = lifter::RV64::Lifter(&ir);
    lifter.lift(&prog);

    if (args.has_argument("print-ir")) {
        std::cout << "------------------------------------------------------------\n";
        std::cout << "IR after Lifting:\n";
        ir.print(std::cout);
        std::cout << "------------------------------------------------------------\n";
    }

    {
        std::vector<std::string> verification_messages;
        if (!ir.verify(verification_messages)) {
            std::cerr << "WARNING: IR irregularities have been found:\n";
            for (const auto &message : verification_messages) {
                std::cerr << "  " << message << '\n';
            }
        }
    }

    generator::x86_64::Generator generator(&ir, std::string(elf_path), output);
    generator.compile();

    if (output != stdout) {
        fclose(output);
    }

    return EXIT_SUCCESS;
}

namespace {
void print_help(bool usage_only) {
    std::cerr << "usage: translate <file> [args...]\n";
    if (!usage_only) {
        std::cerr << "Possible arguments are:\n";
        std::cerr << "    --help:     Shows this help message\n";
        std::cerr << "    --output:   Set the output file name (defaults to stdout)\n";
        std::cerr << "    --debug:    Enables (or disables) debug logging\n";
        std::cerr << "    --print-ir: Prints a textual representation of the IR\n";
        std::cerr << "    --dump-elf: Show information about the input file\n";
    }
}

void dump_elf(const ELF64File *file) {
    if (auto entry_point = file->start_symbol()) {
        std::cout << "    Entry point:     " << std::hex << *entry_point << std::dec << '\n';
    };

    std::cout << "    Sections:        " << file->section_headers.size() << '\n';
    std::cout << "    Program headers: " << file->program_headers.size() << '\n';
    std::cout << "    Symbol count:    " << file->symbol_names.size() << '\n';
}
} // namespace
