#include "argument_parser.h"
#include "common/internal.h"
#include "generator/x86_64/generator.h"
#include "generator/x86_64/linker_script.h"
#include "ir/ir.h"
#include "lifter/elf_file.h"
#include "lifter/lifter.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using std::filesystem::path;

void print_help(bool usage_only);
void dump_elf(const ELF64File *);
std::optional<path> create_temp_directory();
std::optional<path> find_helper_library(const path &exec_dir, const Args &args);
FILE *open_assembler(const path &output_file);
bool run_linker(const path &linker_script_file, const path &output_file, const path &translated_object, const path &helper_library);
} // namespace

int main(int argc, const char **argv) {
    // Parse arguments, excluding the first entry (executable name)
    Args args(argv + 1, argv + argc);

    // TODO This might not work in certain scenarios (symlinks, etc.)
    auto executable_dir = std::filesystem::canonical(argv[0]).parent_path();

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

    path elf_path(args.positional[0]);

    std::cout << "Translating file " << elf_path << '\n';

    auto maybe_temp_dir = create_temp_directory();
    if (!maybe_temp_dir) {
        return EXIT_FAILURE;
    }
    auto temp_dir = *maybe_temp_dir;
    DEBUG_LOG(std::string("Temporary directory is ") + temp_dir.string());

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

    auto binary_image_file = temp_dir / "binary_image.bin";
    {
        FILE *output = fopen(binary_image_file.c_str(), "w");
        if (!output) {
            std::perror("The binary image output file could not be opened");
            return EXIT_FAILURE;
        }

        const error_t r = elf_file->write_binary_image(output);

        fclose(output);

        if (r != EXIT_SUCCESS) {
            std::cerr << "Error: " << r << "\n";
            return EXIT_FAILURE;
        }
    }

    path output_file;
    if (args.has_argument("output")) {
        output_file = path(args.get_argument("output"));
    } else {
        output_file = elf_path;
        output_file.concat(".translated");
    }

    Program prog(std::move(elf_file));

    auto lifter = lifter::RV64::Lifter(&ir);
    lifter.lift(&prog);

    if (args.has_argument("print-ir")) {
        if (auto file = args.get_argument("print-ir"); !file.empty()) {
            std::ofstream out(std::string{file});
            ir.print(out);
        } else {
            std::cout << "------------------------------------------------------------\n";
            std::cout << "IR after Lifting:\n";
            ir.print(std::cout);
            std::cout << "------------------------------------------------------------\n";
        }
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

    auto output_object = temp_dir / "translated.o";
    FILE *assembler = open_assembler(output_object);
    if (!assembler) {
        return EXIT_FAILURE;
    }

    generator::x86_64::Generator generator(&ir, binary_image_file.string(), assembler);
    generator.compile();

    auto asm_status = pclose(assembler);
    if (asm_status != EXIT_SUCCESS) {
        std::cerr << "Assembler failed with exit code " << asm_status << '\n';
        return EXIT_FAILURE;
    }

    auto linker_script_file = temp_dir / "linker.ld";
    {
        std::ofstream linker_script(linker_script_file);
        linker_script << LINKER_SCRIPT;
    }

    auto helper_library = find_helper_library(executable_dir, args);
    if (!helper_library) {
        std::cerr << "The helper library was not found (maybe your directory layout is different).\n";
        std::cerr << "Try setting --helper-path to the path of libhelper.a\n";
        return EXIT_FAILURE;
    }

    if (!run_linker(linker_script_file, output_file, output_object, *helper_library))
        return EXIT_FAILURE;

    std::cout << "Output written to " << output_file << '\n';

    return EXIT_SUCCESS;
}

namespace {
void print_help(bool usage_only) {
    std::cerr << "usage: translate <file> [args...]\n";
    if (!usage_only) {
        std::cerr << "Possible arguments are (--key=value):\n";
        std::cerr << "    --help:        Shows this help message\n";
        std::cerr << "    --output:      Set the output file name (by default, the input file path suffixed with `.translated`)\n";
        std::cerr << "    --debug:       Enables debug logging (use --debug=false to prevent logging in debug builds)\n";
        std::cerr << "    --print-ir:    Prints a textual representation of the IR (if no file is specified, prints to standard out)\n";
        std::cerr << "    --dump-elf:    Show information about the input file\n";
        std::cerr << "    --helper-path: Set the path to the runtime helper library\n";
        std::cerr << "                   (This is only required if the translator can't find it by itself)\n";
        std::cerr << '\n';
        std::cerr << "Environment variables:\n";
        std::cerr << "    AS: Override the assembler binary (by default, the system `as` is used)\n";
        std::cerr << "    LD: Override the linker binary (by default, the system `ld` is used)\n";
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

std::optional<path> create_temp_directory() {
    auto tmp_dir = std::filesystem::temp_directory_path();
    std::string full_path(tmp_dir / "eragp-sbt-XXXXXX");
    // mkdtemp modifies its input argument. data() is always null-terminated since C++11
    if (mkdtemp(full_path.data()) == nullptr) {
        std::perror("mkdtemp");
        return std::nullopt;
    }
    return full_path;
}

std::optional<path> find_helper_library(const path &exec_dir, const Args &args) {
    if (args.has_argument("helper-path")) {
        return path(args.get_argument("helper-path"));
    }
    auto path = exec_dir / "generator/x86_64/helper/libhelper.a";
    if (std::filesystem::exists(path)) {
        return path;
    } else {
        return std::nullopt;
    }
}

inline const char *get_binary(const char *env_name, const char *fallback) {
    const char *env_bin = getenv(env_name);
    return env_bin != nullptr ? env_bin : fallback;
}

FILE *open_assembler(const path &output_file) {
    const char *as = get_binary("AS", "as");

    // Note: operator<< on path quotes its output.
    std::stringstream s;
    s << as << " -c -o " << output_file;
    auto command_line = s.str();

    DEBUG_LOG(std::string("Creating assembler subprocess with: ") + command_line);

    FILE *assembler = popen(command_line.c_str(), "w");
    if (!assembler) {
        std::perror("Failed to spawn assembler");
        return nullptr;
    }
    return assembler;
}

bool run_linker(const path &linker_script_file, const path &output_file, const path &translated_object, const path &helper_library) {
    const char *ld = get_binary("LD", "ld");

    std::stringstream tmp;
    tmp << ld << " -T " << linker_script_file << " -o " << output_file << ' ';
    tmp << translated_object << ' ' << helper_library;
    auto command_line = tmp.str();

    DEBUG_LOG(std::string("Calling linker with: ") + command_line);

    int ld_status = system(command_line.c_str());
    if (ld_status == -1) {
        std::perror("Failed to create linker process");
        return false;
    } else if (ld_status != EXIT_SUCCESS) {
        std::cerr << "The linker failed with exit code " << ld_status << '\n';
        return false;
    }
    return true;
}
} // namespace
