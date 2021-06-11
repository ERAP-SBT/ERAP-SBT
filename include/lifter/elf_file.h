#pragma once

#include <cerrno>
#include <common/internal.h>
#include <cstring>
#include <elf.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

class ELF64File {
  private:
    error_t read_file();

    error_t init_header();

    [[nodiscard]] error_t is_valid_elf_file() const;

    error_t parse_sections();

    error_t parse_program_headers();

    error_t parse_symbols();

  public:
    explicit ELF64File(std::filesystem::path path) : file_path(std::move(path)), header(), section_headers(), section_names(), program_headers(), segment_section_map(), symbols(), symbol_names() {}

    ELF64File() = delete;

    const std::filesystem::path file_path;
    std::vector<uint8_t> file_content;
    Elf64_Ehdr header;

    std::vector<Elf64_Shdr> section_headers;
    std::vector<std::string> section_names;

    // program headers describe segments
    std::vector<Elf64_Phdr> program_headers;

    // index in the map = index in the program_headers vector
    // TODO: evaluate how important this is (the parsing is not near perfect and prone to errors)
    std::vector<std::vector<Elf64_Shdr *>> segment_section_map;

    std::vector<Elf64_Sym> symbols;
    std::vector<std::string> symbol_names;

    error_t parse_elf();

    [[nodiscard]] std::pair<size_t, size_t> bytes_offset(size_t sym_i) const { return bytes_offset(&symbols.at(sym_i)); }

    [[nodiscard]] std::pair<size_t, size_t> bytes_offset(const Elf64_Sym *sym) const {
        const Elf64_Shdr *sec = &section_headers.at(sym->st_shndx);
        return bytes_offset(sym->st_value, sec, sym->st_size);
    }

    [[nodiscard]] static std::pair<size_t, size_t> bytes_offset(uint64_t addr, const Elf64_Shdr *sec, size_t num) {
        if (addr < sec->sh_addr) {
            throw std::invalid_argument("Invalid symbol address.");
        }
        size_t base_offset = addr - sec->sh_addr + sec->sh_offset;
        return std::make_pair(base_offset, num);
    }

    [[nodiscard]] std::optional<size_t> start_symbol() const;
};
