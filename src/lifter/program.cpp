#include <lifter/program.h>

uint64_t Program::load_instrs(uint8_t *byte_arr, size_t n, uint64_t block_start_addr) {
    for (size_t off = 0; off < n;) {
        FrvInst instr;
        off += frv_decode(n - off, byte_arr + off, FRV_RV64, &instr);
        memory[block_start_addr + off] = RV64Inst{instr};
    }
    return block_start_addr + n;
}

uint64_t Program::load_data(const uint8_t *byte_arr, size_t n, uint64_t block_start_addr) {
    for (size_t off = 0; off < n; ++off) {
        memory[block_start_addr + off] = *(byte_arr + off);
    }
    return block_start_addr + n;
}

uint64_t Program::load_symbol_instrs(const std::string &name) {
    auto &sym_names = elf_base->symbol_names;
    auto finding = std::find(sym_names.begin(), sym_names.end(), name);
    if (finding != sym_names.end()) {
        return load_symbol_instrs(finding - sym_names.begin());
    }
    throw std::invalid_argument("Invalid symbol name: not found in elf file.");
}

uint64_t Program::load_symbol_instrs(size_t sym_i) {
    return load_symbol_instrs(elf_base->symbols.at(sym_i));
}

uint64_t Program::load_symbol_instrs(Elf64_Sym sym) {
    return load_instrs(&elf_base->file_content[elf_base->bytes_offset(&sym).first], sym.st_size, sym.st_value);
}

uint64_t Program::load_symbol_data(const std::string &name) {
    auto &sym_names = elf_base->symbol_names;
    auto finding = std::find(sym_names.begin(), sym_names.end(), name);
    if (finding != sym_names.end()) {
        return load_symbol_data(finding - sym_names.begin());
    }
    throw std::invalid_argument("Invalid symbol name: not found in elf file.");
}

uint64_t Program::load_symbol_data(size_t sym_i) {
    return load_symbol_data(elf_base->symbols.at(sym_i));
}

uint64_t Program::load_symbol_data(Elf64_Sym sym) {
    return load_data(&elf_base->file_content[elf_base->bytes_offset(&sym).first], sym.st_size, sym.st_value);
}

uint64_t Program::load_section(const std::string &sec_name) {
    return load_section(std::find(elf_base->section_names.begin(), elf_base->section_names.end(), sec_name) -
                        elf_base->section_names.begin());
}

uint64_t Program::load_section(size_t shdr_i) {
    Elf64_Shdr shdr = elf_base->section_headers.at(shdr_i);

    size_t phdr_i = 0;
    for (size_t i = 0; i < elf_base->segment_section_map.size(); i++) {
        for (size_t j = 0; j < elf_base->segment_section_map.at(i).size(); j++) {
            if (elf_base->segment_section_map.at(i).at(j) == &shdr) {
                phdr_i = i;
                break;
            }
        }
    }
    Elf64_Phdr phdr = elf_base->program_headers.at(phdr_i);

    if (shdr.sh_flags & SHF_EXECINSTR || phdr.p_flags & PF_X) {
        for (Elf64_Sym sym : elf_base->symbols) {
            if (sym.st_shndx == shdr_i) {
                load_symbol_instrs(sym);
            }
        }
    } else if (phdr.p_flags & PF_W || phdr.p_flags & PF_R) {
        for (Elf64_Sym sym : elf_base->symbols) {
            if (sym.st_shndx == shdr_i) {
                load_symbol_data(sym);
            }
        }
    } else {
        throw std::invalid_argument("Currently unsupported section: \"" + elf_base->section_names.at(shdr_i) + "\"");
    }
    return shdr.sh_addr;
}

void Program::resolve_relative(uint64_t start_addr, uint64_t end_addr) {
    for (auto it = memory.find(start_addr); it != memory.find(end_addr); it++) {
        // TODO: parse instruction
        if (it->second.index() == 1) {

        }
    }
}
