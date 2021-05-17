#pragma once

#include <lifter/elf_file.h>
#include <map>
#include <variant>
#include <algorithm>

struct RV64Inst {
    FrvInst instr;
    uint64_t virt_addr{0};
};

/*
 * The "program" is used for loading a higher representation of the elf binary files.
 */
struct Program {
    explicit Program(std::unique_ptr<ELF64File> file_ptr) : elf_base(std::move(file_ptr)), memory() {};

    uint64_t load_instrs(uint8_t *, size_t, uint64_t);

    uint64_t load_data(const uint8_t *, size_t, uint64_t);

    // load RV64 instructions from symbol
    uint64_t load_symbol_instrs(size_t);

    uint64_t load_symbol_instrs(const std::string &);

    uint64_t load_symbol_instrs(Elf64_Sym);

    // load data from section and guess the format.
    // Sections which are executable are loaded as instructions, other read / write sections are data.
    uint64_t load_section(const std::string &sec_name);

    uint64_t load_section(size_t);

    // load binary data from elf file symbol
    uint64_t load_symbol_data(size_t);

    uint64_t load_symbol_data(const std::string &);

    uint64_t load_symbol_data(Elf64_Sym);

    // resolve ip-relative addresses and immediates
    void resolve_relative(uint64_t, uint64_t);

    // the elf binary which contains the raw program data
    const std::unique_ptr<ELF64File> elf_base;

    // The memory maps virtual addresses to RV64 instructions and raw data.
    // if this map draws too much performance, i can always be replaced with two vectors
    std::map<uint64_t, std::variant<std::monostate, RV64Inst, uint8_t>> memory;
};
