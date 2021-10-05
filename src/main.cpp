#include "argument_parser.h"
#include "common/internal.h"
#include "generator/x86_64/generator.h"
#include "ir/ir.h"
#include "ir/optimizer/common.h"
#include "ir/optimizer/const_folding.h"
#include "ir/optimizer/dce.h"
#include "ir/optimizer/dedup.h"
#include "lifter/elf_file.h"
#include "lifter/lifter.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using std::filesystem::path;

void print_help(bool usage_only);
void parse_opt_flags(const Args &args, uint32_t &ir_optimizations, uint32_t &gen_optimizations);
void dump_elf(const ELF64File *);
std::optional<path> create_temp_directory();
bool find_runtime_dependencies(const path &exec_dir, const Args &args, path &out_helper_lib, path &out_linker_script);
FILE *open_assembler(const path &output_file);
bool run_linker(const path &linker_script_file, const path &output_file, const path &translated_object, const path &helper_library);
} // namespace

int main(int argc, const char **argv) {
    using namespace std::chrono;
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

    if (args.has_argument("transform-call-ret")) {
        ENABLE_CALL_RET_TRANSFORM = args.get_value_as_bool("transform-call-ret");
    }

    if (args.has_argument("full-backtracking")) {
        FULL_BACKTRACKING = args.get_value_as_bool("full-backtracking");
    }

    if (args.positional.empty()) {
        std::cerr << "Missing input file argument!\n";
        print_help(true);
        return EXIT_FAILURE;
    }

    uint32_t ir_optimizations = 0, gen_optimizations = 0;
    parse_opt_flags(args, ir_optimizations, gen_optimizations);

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

    const bool interpreter_only = args.has_argument("interpreter-only") && (args.get_argument("interpreter-only") == "" || args.get_value_as_bool("interpreter-only"));
    const auto time_pre_lift = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

    Program prog(std::move(elf_file));
    // support floating points if the flag isn't set or the provided value isn't equal to true
    const bool fp_support = !args.has_argument("disable-fp") || (args.get_argument("disable-fp") != "" && !args.get_value_as_bool("disable-fp"));

    auto lifter = lifter::RV64::Lifter(&ir, fp_support, interpreter_only);
    lifter.lift(&prog);
    const auto time_post_lift = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

    signal(SIGPIPE, SIG_IGN);

    {
        if (ir_optimizations & optimizer::OPT_CONST_FOLDING) {
            optimizer::const_fold(&ir);
        }
        if (ir_optimizations & optimizer::OPT_DCE) {
            optimizer::dce(&ir);
        }
        if (ir_optimizations & optimizer::OPT_DEDUP) {
            optimizer::dedup(&ir);
        }
    }

    {
        std::vector<std::string> verification_messages;
        if (!ir.verify(verification_messages)) {
            std::cerr << "WARNING: IR irregularities have been found:\n";
            for (const auto &message : verification_messages) {
                std::cerr << "  " << message << '\n';
            }
            if (args.get_value_as_bool("allow-inconsistency")) {
                std::cerr << "Ignoring inconsistencies as told, but this might lead to errors later on\n";
            } else {
                return EXIT_FAILURE;
            }
        }
    }

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

    auto output_object = temp_dir / "translated.o";
    FILE *assembler = open_assembler(output_object);
    if (!assembler) {
        return EXIT_FAILURE;
    }
    uint64_t time_pre_gen, time_post_gen;
    if (!args.has_argument("asm-out")) {
        time_pre_gen = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        generator::x86_64::Generator generator(&ir, binary_image_file.string(), assembler, interpreter_only);
        generator.optimizations = gen_optimizations;
        generator.compile();
        time_post_gen = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    } else {
        const auto asm_file = std::string{args.get_argument("asm-out")};
        auto asm_out = fopen(asm_file.c_str(), "w");
        if (!asm_out) {
            std::cerr << "The assembly output couldn't be opened: " << std::strerror(errno) << "\n";
            return EXIT_FAILURE;
        }

        time_pre_gen = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        generator::x86_64::Generator generator(&ir, binary_image_file.string(), asm_out, interpreter_only);
        generator.optimizations = gen_optimizations;
        generator.compile();
        time_post_gen = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        const auto file_size = ftell(asm_out);
        fclose(asm_out);

        asm_out = fopen(asm_file.c_str(), "r");
        if (!asm_out) {
            return EXIT_FAILURE;
        }

        const auto asm_out_fd = fileno(asm_out);
        const auto assembler_fd = fileno(assembler);
        auto bytes_left = file_size;
        while (true) {
            const auto res = splice(asm_out_fd, nullptr, assembler_fd, nullptr, bytes_left, 0);
            if (res < 0) {
                std::cerr << "Failed to assemble the binary: " << std::strerror(errno) << '\n';
                return EXIT_FAILURE;
            }
            bytes_left -= res;
            if (res == 0 || bytes_left == 0) {
                break;
            }
        }

        fclose(asm_out);
    }

    auto asm_status = pclose(assembler);
    if (asm_status != EXIT_SUCCESS) {
        std::cerr << "Assembler failed with exit code " << asm_status << '\n';
        return EXIT_FAILURE;
    }

    path helper_library, linker_script_file;
    if (!find_runtime_dependencies(executable_dir, args, helper_library, linker_script_file))
        return EXIT_FAILURE;

    if (!run_linker(linker_script_file, output_file, output_object, helper_library))
        return EXIT_FAILURE;

    std::cout << "Output written to " << output_file << '\n';
    std::cout << "Lifting took " << (time_post_lift - time_pre_lift) << "ms\n";
    std::cout << "Generating took " << (time_post_gen - time_pre_gen) << "ms\n";

    return EXIT_SUCCESS;
}

namespace {
void print_help(bool usage_only) {
    std::cerr << "usage: translate <file> [args...]\n";
    if (!usage_only) {
        std::cerr << "Possible arguments are (--key=value):\n";
        std::cerr << "    --asm-out:                Output the generated Assembly to a file\n";
        std::cerr << "    --debug:                  Enables debug logging (use --debug=false to prevent logging in debug builds)\n";
        std::cerr << "    --disable-fp:             Disables the support of floating point instructions.\n";
        std::cerr << "    --dump-elf:               Show information about the input file\n";
        std::cerr << "    --full-backtracking:      Evaluates every possible input combination for indirect jump address backtracking.\n";
        std::cerr << "    --help:                   Shows this help message\n";
        std::cerr << "    --interpreter-only:       Only uses the interpreter to translate the binary (dynamic binary translation). (default: false)\n";
        std::cerr << "    --optimize:               Set optimization flags, comma-seperated list. Specifying a group enables all flags in that group. Appending '!' before disables a single flag\n";
        std::cerr << "    Optimization Flags:\n";
        std::cerr << "      - ir:\n";
        std::cerr << "          - dce: Dead Code Elimination\n";
        std::cerr << "          - const_folding: Fold and propagage constant values\n";
        std::cerr << "          - dedup: Deduplicate variables\n";
        std::cerr << "      - generator:\n";
        std::cerr << "          - reg_alloc:            Register Allocation\n";
        std::cerr << "          - merge_ops:            Merge multiple IR-Operations into a single native op\n";
        std::cerr << "          - unused_statics:       Eliminate unused static-load-stores in the default generator\n";
        std::cerr << "          - bmi2:                 Allow usage of instructions in the BMI2 instruction set extension (shlx/shrx/sarx)\n";
        std::cerr << "          - inline_syscalls:      Inline system calls when their ID is known\n";
        std::cerr << "    --output:                 Set the output file name (by default, the input file path suffixed with `.translated`)\n";
        std::cerr << "    --print-ir:               Prints a textual representation of the IR (if no file is specified, prints to standard out)\n";
        std::cerr << "    --transform-call-ret:     Detect and replace RISC-V `call` and `return` instructions\n\n";
        std::cerr << "    --helper-path:            Set the path to the runtime helper library\n";
        std::cerr << "    --linkerscript-path:      Set the path to the linker script\n";
        std::cerr << "                              (The above two are only required if the translator can't find these by itself)\n\n";
        std::cerr << "    --allow-inconsistency:    Allow inconsistencies in the IR (not recommended).\n";
        std::cerr << '\n';
        std::cerr << "Environment variables:\n";
        std::cerr << "    AS: Override the assembler binary (by default, the system `as` is used)\n";
        std::cerr << "    LD: Override the linker binary (by default, the system `ld` is used)\n";
    }
}

void parse_opt_flags(const Args &args, uint32_t &ir_optimizations, uint32_t &gen_optimizations) {
    if (!args.has_argument("optimize")) {
        // TODO: turn on flags by default?
        return;
    }
    auto val = args.get_argument("optimize");
    while (true) {
        const auto comma_pos = val.find_first_of(',');
        auto opt_flag = val.substr(0, comma_pos);
        if (opt_flag.empty()) {
            if (comma_pos == std::string::npos) {
                break;
            }
            continue;
        }

        auto disable_flag = false;
        if (opt_flag[0] == '!' || opt_flag[0] == '-') {
            disable_flag = true;
            opt_flag.remove_prefix(1);
        }

        uint32_t gen_opt_change = 0, ir_opt_change = 0;
        if (opt_flag == "all") {
            gen_opt_change = 0xFFFFFFFF;
            ir_opt_change = optimizer::OPT_FLAGS_ALL;
        } else if (opt_flag == "generator") {
            gen_opt_change = 0xFFFFFFFF;
        } else if (opt_flag == "ir") {
            ir_opt_change = optimizer::OPT_FLAGS_ALL;
        } else if (opt_flag == "dce") {
            ir_opt_change = optimizer::OPT_DCE;
        } else if (opt_flag == "const_folding") {
            ir_opt_change = optimizer::OPT_CONST_FOLDING | optimizer::OPT_DCE; // Constant folding requires DCE
        } else if (opt_flag == "dedup_imm") {
            ir_opt_change = optimizer::OPT_DEDUP;
        } else if (opt_flag == "reg_alloc") {
            gen_opt_change = generator::x86_64::Generator::OPT_MBRA;
        } else if (opt_flag == "unused_statics") {
            gen_opt_change = generator::x86_64::Generator::OPT_UNUSED_STATIC;
        } else if (opt_flag == "merge_ops") {
            gen_opt_change = generator::x86_64::Generator::OPT_MERGE_OP;
        } else if (opt_flag == "bmi2") {
            gen_opt_change = generator::x86_64::Generator::OPT_ARCH_BMI2;
        } else if (opt_flag == "inline_syscalls") {
            gen_opt_change = generator::x86_64::Generator::OPT_INLINE_SYSCALLS;
        } else {
            std::cerr << "Warning: Unknown optimization flag: '" << opt_flag << "'\n";
        }

        if (disable_flag) {
            ir_optimizations &= ~ir_opt_change;
            gen_optimizations &= ~gen_opt_change;
        } else {
            ir_optimizations |= ir_opt_change;
            gen_optimizations |= gen_opt_change;
        }

        if (comma_pos == std::string::npos) {
            break;
        }
        val.remove_prefix(comma_pos + 1);
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

bool find_runtime_dependencies(const path &exec_dir, const Args &args, path &out_helper_lib, path &out_linker_script) {
    if (args.has_argument("helper-path")) {
        out_helper_lib = path(args.get_argument("helper-path"));
    } else {
        // If the translator is in a build environment, the helper library is at <bin_dir>/generator/x86_64/helper/libhelper-x86_64.a
        auto hlp_path = exec_dir / "generator/x86_64/helper/libhelper-x86_64.a";
        if (std::filesystem::exists(hlp_path)) {
            out_helper_lib = hlp_path;
        } else {
            // If the translator is in a install environment ("meson install"), the helper library is at <PREFIX>/share/eragp-sbt-2021/libhelper-x86_64.a
            hlp_path = std::filesystem::weakly_canonical(exec_dir / "../share/eragp-sbt-2021/libhelper-x86_64.a");
            if (std::filesystem::exists(hlp_path)) {
                out_helper_lib = hlp_path;
            } else {
                std::cerr << "The helper library was not found (maybe your directory layout is different).\n";
                std::cerr << "Try setting --helper-path to the path of libhelper-x86_64.a\n";
                return false;
            }
        }
    }

    if (args.has_argument("linkerscript-path")) {
        out_linker_script = path(args.get_argument("linkerscript-path"));
    } else {
        // Build environment: Assume that the build directory is a subdirectory of the source root
        auto lds_path = std::filesystem::weakly_canonical(exec_dir / "../../src/generator/x86_64/helper/link.ld");
        if (std::filesystem::exists(lds_path)) {
            out_linker_script = lds_path;
        } else {
            // Install environment: link.ld is at <PREFIX>/share/eragp-sbt-2021/link.ld
            lds_path = std::filesystem::weakly_canonical(exec_dir / "../share/eragp-sbt-2021/link.ld");
            if (std::filesystem::exists(lds_path)) {
                out_linker_script = lds_path;
            } else {
                std::cerr << "The linker script was not found (maybe your directory layout is different).\n";
                std::cerr << "Try setting --linkerscript-path to the path of link.ld\n";
                return false;
            }
        }
    }

    return true;
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
