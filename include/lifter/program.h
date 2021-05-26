#pragma once

#include <algorithm>
#include <lifter/elf_file.h>
#include <map>
#include <variant>

struct RV64Inst {
    // the base decoded instruction
    FrvInst instr;

    // the instruction size in bytes
    size_t size;
};

/*
 * The "program" is used for loading a higher representation of the elf binary files.
 */
struct Program {
    explicit Program(std::unique_ptr<ELF64File> file_ptr) : elf_base(std::move(file_ptr)), addrs(), data(){};

    uint64_t load_instrs(uint8_t *, size_t, uint64_t);

    uint64_t load_data(const uint8_t *, size_t, uint64_t);

    // load RV64 instructions from symbol
    uint64_t load_symbol_instrs(size_t);

    uint64_t load_symbol_instrs(const std::string &);

    uint64_t load_symbol_instrs(Elf64_Sym *);

    // load data from section and guess the format.
    // Sections which are executable are loaded as instructions, other read / write sections are data.
    uint64_t load_section(const std::string &sec_name);

    uint64_t load_section(Elf64_Shdr *);

    uint64_t load_section(size_t);

    // load binary data from elf file symbol
    uint64_t load_symbol_data(size_t);

    uint64_t load_symbol_data(const std::string &);

    uint64_t load_symbol_data(Elf64_Sym *);

    // the elf binary which contains the raw program data
    const std::unique_ptr<ELF64File> elf_base;

    // These two vectors map virtual addresses to RV64 instructions and raw data.
    std::vector<uint64_t> addrs;
    std::vector<std::variant<std::monostate, RV64Inst, uint8_t>> data;

    std::vector<uint64_t>::difference_type insert_addr(uint64_t addr) {
        auto it = std::lower_bound(addrs.begin(), addrs.end(), addr);
        auto idx = (it - addrs.begin());
        addrs.insert(it, addr);
        return idx;
    }

    void insert_value(uint64_t addr, RV64Inst value) { data.emplace((data.begin() + insert_addr(addr)), value); }

    void insert_value(uint64_t addr, uint8_t value) { data.emplace((data.begin() + insert_addr(addr)), value); }
};
