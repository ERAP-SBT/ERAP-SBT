#include <lifter/program.h>

uint64_t Program::load_instrs(uint8_t *byte_arr, size_t n, uint64_t block_start_addr) {
    int return_code = 0;
    for (size_t off = 0; off < n; off += return_code) {
        FrvInst instr;
        return_code = frv_decode(n - off, byte_arr + off, FRV_RV64, &instr);
        if (return_code <= 0) {
#ifdef DEBUG
            std::cerr << "Discovered partial, invalid or undefined instruction at address <0x" << std::hex << (block_start_addr + off) << ">. Skipping (+ 2)\n";
#endif
            return_code = 2;
            instr.mnem = FRV_INVALID;
        }
        insert_value(block_start_addr + off, RV64Inst{instr, (size_t)return_code});
    }
    return block_start_addr + n;
}

uint64_t Program::load_data(const uint8_t *byte_arr, size_t n, uint64_t block_start_addr) {
    for (size_t off = 0; off < n; ++off) {
        insert_value(block_start_addr + off, *(byte_arr + off));
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

uint64_t Program::load_symbol_instrs(size_t sym_i) { return load_symbol_instrs(&elf_base->symbols.at(sym_i)); }

uint64_t Program::load_symbol_instrs(Elf64_Sym *sym) {
    if (!sym->st_size) {
        std::cerr << "Trying to parse a symbol with unknown or no size. Searching for endpoint...";
        uint64_t next_sym_addr = UINT64_MAX;
        for (auto &other_sym : elf_base->symbols) {
            if (other_sym.st_value > sym->st_value && other_sym.st_value < next_sym_addr) {
                next_sym_addr = other_sym.st_value;
            }
        }
        sym->st_size = next_sym_addr - sym->st_value;
        std::cerr << " found endpoint: <" << std::hex << next_sym_addr << "> (size: " << sym->st_size << ")\n";
    }
    if (ELF32_ST_TYPE(sym->st_info) & STT_NOTYPE) {
        std::cerr << "Trying to parse a STT_NOTYPE symbol. This is not supported."
                  << "\n";
        return sym->st_value;
    }
    if (sym->st_value < elf_base->section_headers.at(sym->st_shndx).sh_addr) {
        std::cout << "Symbol address outside of associated section. This doesn't seem right..."
                  << "\n";
        return sym->st_value;
    }
    return load_instrs(&elf_base->file_content[elf_base->bytes_offset(sym).first], sym->st_size, sym->st_value);
}

uint64_t Program::load_symbol_data(const std::string &name) {
    auto &sym_names = elf_base->symbol_names;
    auto finding = std::find(sym_names.begin(), sym_names.end(), name);
    if (finding != sym_names.end()) {
        return load_symbol_data(finding - sym_names.begin());
    }
    throw std::invalid_argument("Invalid symbol name: not found in elf file.");
}

uint64_t Program::load_symbol_data(size_t sym_i) { return load_symbol_data(&elf_base->symbols.at(sym_i)); }

uint64_t Program::load_symbol_data(Elf64_Sym *sym) {
    if (!sym->st_size) {
        std::cerr << "Trying to parse a symbol with unknown or no size. This is currently not supported."
                  << "\n";
        return sym->st_value;
    }
    if (ELF32_ST_TYPE(sym->st_info) & STT_NOTYPE) {
        std::cerr << "Trying to parse a STT_NOTYPE symbol. This is not supported."
                  << "\n";
        return sym->st_value;
    }
    if (sym->st_value < elf_base->section_headers.at(sym->st_shndx).sh_addr) {
        std::cout << "Symbol address outside of associated section. This doesn't seem right..."
                  << "\n";
        return sym->st_value;
    }
    return load_data(&elf_base->file_content[elf_base->bytes_offset(sym).first], sym->st_size, sym->st_value);
}

uint64_t Program::load_section(Elf64_Shdr *sec) {
    size_t shdr_i = 0;
    for (; shdr_i < elf_base->section_headers.size(); shdr_i++) {
        if (elf_base->section_headers.at(shdr_i).sh_name == sec->sh_name) {
            break;
        }
    }
    if (shdr_i >= elf_base->section_headers.size()) {
        throw std::invalid_argument("Can't find supplied section in elf file sections.");
    }
    return load_section(shdr_i);
}

uint64_t Program::load_section(const std::string &sec_name) {
    return load_section(std::find(elf_base->section_names.begin(), elf_base->section_names.end(), sec_name) - elf_base->section_names.begin());
}

uint64_t Program::load_section(size_t shdr_i) {
    Elf64_Shdr &shdr = elf_base->section_headers.at(shdr_i);

    size_t phdr_i = 0;
    for (auto &mapping : elf_base->segment_section_map) {
        if ((phdr_i = std::find(mapping.begin(), mapping.end(), &shdr) - mapping.begin()) < mapping.size()) {
            break;
        }
    }
    bool check_phdr = true;
    if (phdr_i >= elf_base->program_headers.size()) {
        std::cerr << "Can't find program header for section \"" << elf_base->section_names[shdr_i] << "\", ignoring header checks.\n";
        check_phdr = false;
    }
    Elf64_Phdr *phdr = (check_phdr) ? &elf_base->program_headers.at(phdr_i) : nullptr;

    if (shdr.sh_flags & SHF_EXECINSTR || (check_phdr && phdr->p_flags & PF_X)) {
        for (Elf64_Sym &sym : elf_base->symbols) {
            if (sym.st_shndx == shdr_i) {
                load_symbol_instrs(&sym);
            }
        }
    } else if (shdr.sh_flags & SHF_ALLOC || shdr.sh_flags & SHF_WRITE || (check_phdr && phdr->p_flags & PF_W) || (check_phdr && phdr->p_flags & PF_R)) {
        for (Elf64_Sym &sym : elf_base->symbols) {
            if (sym.st_shndx == shdr_i) {
                load_symbol_data(&sym);
            }
        }
    } else {
        throw std::invalid_argument("Currently unsupported section: \"" + elf_base->section_names.at(shdr_i) + "\"");
    }
    return shdr.sh_addr;
}
