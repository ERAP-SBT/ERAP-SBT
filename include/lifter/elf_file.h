#pragma once

#include <frvdec.h>
#include <elf.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

#ifdef DEBUG
#define DEBUG_LOG(string) std::cout << "<debug>: " << string << "\n";
#else
#define DEBUG_LOG(string)
#endif


class ELF64File {
private:
    void read_file();

    void init_header();

    [[nodiscard]] bool is_valid_elf_file() const;

    void parse_sections();

    void parse_program_headers();

    void parse_symbols();

public:
    explicit ELF64File(std::filesystem::path);

    ELF64File() = delete;

    std::filesystem::path file_path;
    std::vector<uint8_t> file_content;
    Elf64_Ehdr header{};

    std::vector<Elf64_Shdr> section_headers{};
    std::vector<std::string> section_names{};

    // program headers describe segments
    std::vector<Elf64_Phdr> program_headers{};
    // index in the map = index in the program_headers vector
    // TODO: evaluate how important this is (the parsing is not near perfect and prune to errors)
    std::vector<std::vector<Elf64_Shdr *>> segment_section_map;

    std::vector<Elf64_Sym> symbols{};
    std::vector<std::string> symbol_names{};

    [[nodiscard]] std::vector<uint8_t> bytes_at_symbol(size_t sym_i) const {
        return bytes_at_symbol(symbols.at(sym_i));
    }

    [[nodiscard]] std::vector<uint8_t> bytes_at_symbol(Elf64_Sym sym) const {
        Elf64_Shdr sec = section_headers.at(sym.st_shndx);
        return bytes_at_addr(sym.st_value, sec, sym.st_size);
    }

    [[nodiscard]] std::vector<uint8_t> bytes_at_addr(uint64_t addr, Elf64_Shdr sec, size_t num) const {
        uint64_t base_offset = addr - sec.sh_addr + sec.sh_offset;
        return std::vector<uint8_t>(&file_content[base_offset], &file_content[base_offset + num]);
    }

    [[nodiscard]] size_t start_symbol() const;
};