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
    return load_symbol_instrs(&elf_base->symbols.at(sym_i));
}

uint64_t Program::load_symbol_instrs(Elf64_Sym *sym) {
    if (!sym->st_size) {
        std::cerr << "Trying to parse a symbol with unknown or no size. This is currently not supported." << "\n";
        return sym->st_value;
    }
    if (ELF32_ST_TYPE(sym->st_info) & STT_NOTYPE) {
        std::cerr << "Trying to parse a STT_NOTYPE symbol. This is not supported." << "\n";
        return sym->st_value;
    }
    if (sym->st_value < elf_base->section_headers.at(sym->st_shndx).sh_addr) {
        std::cout << "Symbol address outside of associated section. This doesn't seem right..." << "\n";
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

uint64_t Program::load_symbol_data(size_t sym_i) {
    return load_symbol_data(&elf_base->symbols.at(sym_i));
}

uint64_t Program::load_symbol_data(Elf64_Sym *sym) {
    if (!sym->st_size) {
        std::cerr << "Trying to parse a symbol with unknown or no size. This is currently not supported." << "\n";
        return sym->st_value;
    }
    if (ELF32_ST_TYPE(sym->st_info) & STT_NOTYPE) {
        std::cerr << "Trying to parse a STT_NOTYPE symbol. This is not supported." << "\n";
        return sym->st_value;
    }
    if (sym->st_value < elf_base->section_headers.at(sym->st_shndx).sh_addr) {
        std::cout << "Symbol address outside of associated section. This doesn't seem right..." << "\n";
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
    return load_section(std::find(elf_base->section_names.begin(), elf_base->section_names.end(), sec_name) -
                        elf_base->section_names.begin());
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
        std::cerr << "Can't find program header for section \"" << elf_base->section_names[shdr_i]
                  << "\", ignoring header checks.\n";
        check_phdr = false;
    }
    Elf64_Phdr *phdr = (check_phdr) ? &elf_base->program_headers.at(phdr_i) : nullptr;

    if (shdr.sh_flags & SHF_EXECINSTR || (check_phdr && phdr->p_flags & PF_X)) {
        for (Elf64_Sym &sym : elf_base->symbols) {
            if (sym.st_shndx == shdr_i) {
                load_symbol_instrs(&sym);
            }
        }
    } else if (shdr.sh_flags & SHF_ALLOC || shdr.sh_flags & SHF_WRITE || (check_phdr && phdr->p_flags & PF_W) ||
               (check_phdr && phdr->p_flags & PF_R)) {
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

void Program::resolve_relative(uint64_t start_addr, uint64_t end_addr) {
    // first, find the start and end places in the map (addresses might be slightly off)
    std::map<uint64_t, std::variant<std::monostate, RV64Inst, uint8_t>>::iterator start_iter;
    std::map<uint64_t, std::variant<std::monostate, RV64Inst, uint8_t>>::iterator end_iter;
    for (size_t i = start_addr; i <= end_addr; i++) {
        start_iter = memory.find(i);
        if (start_iter != memory.end())
            break;
    }
    for (size_t i = end_addr; i <= end_addr + 0x1; i++) {
        end_iter = memory.find(i);
        if (end_iter != memory.end())
            break;
    }
    while (start_iter != end_iter && start_iter != memory.end()) {
        if (start_iter->second.index() == 1) {
            auto &instr = std::get<RV64Inst>(start_iter->second);
            switch (instr.instr.mnem) {
                case FRV_JAL:
                case FRV_JALR:  // here, the address will be added to rs1
                case FRV_BEQ:
                case FRV_BGE:
                case FRV_BGEU:
                case FRV_BLT:
                case FRV_BLTU:
                case FRV_BNE:
                case FRV_LB:
                case FRV_LH:
                case FRV_LW:
                case FRV_LD:
                case FRV_LBU:
                case FRV_LHU:
                case FRV_LWU:
                case FRV_SB:
                case FRV_SH:
                case FRV_SW:
                case FRV_SD:
                    instr.virt_addr = instr.instr.imm + start_iter->first;
                    break;

                case FRV_AUIPC:
                    instr.imm = instr.instr.imm + start_iter->first;
                    break;
            }
        }
        start_iter++;
    }
}
