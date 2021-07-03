#include "lifter/elf_file.h"

error_t ELF64File::parse_elf() {
    DEBUG_LOG("Start reading ELF file.");
    error_t e_code = 0;

    // read binary file into byte vector `file_content`
    if ((e_code = read_file())) {
        return e_code;
    }

    // extract file header
    if ((e_code = init_header())) {
        return e_code;
    }

    // test various characteristics which are required for successful parsing
    if ((e_code = is_valid_elf_file())) {
        return e_code;
    }

    // Only sections if the elf files contains any
    if (header.e_shoff != 0) {
        if ((e_code = parse_sections())) {
            return e_code;
        }
    }

    // elf files should always contain program headers
    if (header.e_phoff != 0) {
        if ((e_code = parse_program_headers())) {
            return e_code;
        }

        // If the elf file is stripped, this won't add any symbols
        if ((e_code = parse_symbols())) {
            return e_code;
        }
    } else {
        std::cerr << "Elf file doesn't contain program headers which are required.\n";
        return ENOEXEC;
    }

    DEBUG_LOG("Done reading valid ELF file.");
    return e_code;
}

error_t ELF64File::is_valid_elf_file() const {
    // ELF file test
    if (header.e_ident[0] != ELFMAG0 || header.e_ident[1] != ELFMAG1 || header.e_ident[2] != ELFMAG2 || header.e_ident[3] != ELFMAG3) {
        std::cout << "Invalid ELF file (wrong file identification byte(s)).\n";
        return ENOEXEC;
    }

    // 64-Bit test
    if (header.e_type != ELFCLASS64) {
        std::cout << "Invalid ELF file (only 64-bit ELF files are currently supported).\n";
        return ENOEXEC;
    }

    // Little-endian test
    if (file_content.at(EI_DATA) != ELFDATA2LSB) {
        std::cout << "Invalid ELF file (only little-endian ELF files are currently supported).\n";
        return ENOEXEC;
    }

    // RISC-V operating system test (unknown machine is possible for stripped ELF binaries)
    if (header.e_machine != EM_RISCV && header.e_machine != EM_NONE) {
        std::cout << "Invalid ELF file (only RISC-V ELF files are supported).\n";
        return ENOEXEC;
    }

    // System V ABI test
    if (file_content.at(EI_OSABI) != ELFOSABI_SYSV && file_content.at(EI_OSABI) != ELFOSABI_LINUX) {
        std::cout << "Invalid ELF file (only ELF files which were compiled for the Unix / System V ABI are currently supported).\n";
        return ENOEXEC;
    }

    // Executable test
    if (header.e_type != ET_EXEC) {
        std::cout << "Invalid ELF file (only executable ELF files are supported).\n";
        return ENOEXEC;
    }
    return EXIT_SUCCESS;
}

error_t ELF64File::read_file() {
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "Error during opening of file \"" << file_path.string() << "\": file does not exist.\n";
        return ENOENT;
    }

    std::ifstream i_stream;
    i_stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        i_stream.open(file_path, std::ios::in | std::ios::binary);
    } catch (const std::ifstream::failure &error) {
        std::cerr << "Error during opening of file \"" << file_path.string() << "\": " << error.what() << "\n";
        return error.code().value();
    }

    if (!i_stream.is_open()) {
        std::cerr << "Unknown error during opening of file.\n";
        return EIO;
    }
    file_content = std::vector<uint8_t>((std::istreambuf_iterator<char>(i_stream)), std::istreambuf_iterator<char>());
    return EXIT_SUCCESS;
}

error_t ELF64File::parse_sections() {
    // Check for potential buffer overflows
    if (header.e_shentsize != sizeof(Elf64_Shdr)) {
        std::cerr << "Invalid elf file section header size.\n";
        return ENOEXEC;
    }

    for (size_t i = 0; i < header.e_shnum; ++i) {
        Elf64_Shdr shdr;

        // manually copy bytes from file into section header struct
        std::memcpy(&shdr, file_content.data() + header.e_shoff + (header.e_shentsize * i), sizeof(Elf64_Shdr));
        section_headers.push_back(shdr);
    }

    // SHN_UNDEF == no string table
    if (header.e_shstrndx != SHN_UNDEF) {
        size_t str_tbl_ind = header.e_shstrndx;

        // set if the index was too big for the header (e_shstrndx > SHN_LORESERVE)
        if (str_tbl_ind == SHN_XINDEX) {
            str_tbl_ind = section_headers.at(0).sh_link;
        }

        if (section_headers.at(str_tbl_ind).sh_type != SHT_STRTAB) {
            std::cerr << "Invalid section type: referenced string table section is not of type <STRTAB>.\n";
            return ENOEXEC;
        }
        const char *str_tbl = reinterpret_cast<const char *>(file_content.data() + section_headers.at(str_tbl_ind).sh_offset);
        for (auto curr_hdr : section_headers) {
            section_names.emplace_back(str_tbl + curr_hdr.sh_name);
        }
    } else {
        DEBUG_LOG("No section name string table declared in ELF file, skipping.");
    }
    return EXIT_SUCCESS;
}

error_t ELF64File::parse_program_headers() {
    if (header.e_phentsize != sizeof(Elf64_Phdr)) {
        std::cerr << "Invalid elf file program header size.\n";
        return ENOEXEC;
    }

    for (size_t i = 0; i < header.e_phnum; ++i) {
        Elf64_Phdr phdr;
        std::memcpy(&phdr, file_content.data() + header.e_phoff + (header.e_phentsize * i), sizeof(Elf64_Phdr));
        program_headers.push_back(phdr);

        if (phdr.p_type == PT_INTERP) {
            std::cerr << "The input file features information for an interpreter. This type of ELF file is not supported.\n";
            return ENOEXEC;
        } else if (phdr.p_type == PT_DYNAMIC) {
            std::cerr << "The input file features dynamic linking information. This type of ELF file is not supported.\n";
            return ENOEXEC;
        }

        auto &program_map = segment_section_map.emplace_back();
        // rough approximation, for further reference:
        // https://github.com/bminor/binutils-gdb/blob/4ba8500d63991518aefef86474576de565e00237/include/elf/internal.h#L316
        for (auto it = section_headers.begin(); it != section_headers.end(); it++) {
            if (it->sh_offset >= phdr.p_offset && it->sh_offset + it->sh_size <= phdr.p_offset + phdr.p_filesz) {
                program_map.push_back(it.base());
            }
        }
    }
    return EXIT_SUCCESS;
}

error_t ELF64File::parse_symbols() {
    if (section_headers.empty()) {
        std::cerr << "No section headers were found in the ELF-File. This is currently not supported.\n";
        return ENOTSUP;
    }

    size_t sym_tbl_i = SIZE_MAX;
    size_t sym_str_tbl_i = SIZE_MAX;
    for (size_t i = 0; i < section_headers.size(); i++) {
        if (section_headers.at(i).sh_type == SHT_SYMTAB) {
            sym_tbl_i = i;
            break;
        }
    }
    // a symbol section should be present in the current file
    if (sym_tbl_i == SIZE_MAX) {
        std::cerr << "No symbol section was found in the ELF file. Skipping symbol parsing.\n";
        return EXIT_SUCCESS;
    }
    Elf64_Shdr sym_tbl = section_headers.at(sym_tbl_i);

    for (size_t i = 0; i < sym_tbl.sh_size / sym_tbl.sh_entsize; ++i) {
        Elf64_Sym sym;
        std::memcpy(&sym, file_content.data() + sym_tbl.sh_offset + (sym_tbl.sh_entsize * i), sizeof(Elf64_Sym));
        symbols.push_back(sym);
    }

    sym_str_tbl_i = sym_tbl.sh_link;
    if (sym_str_tbl_i) {
        Elf64_Shdr sym_str_tbl = section_headers.at(sym_str_tbl_i);
        auto str_tbl_start = reinterpret_cast<const char *>(file_content.data() + sym_str_tbl.sh_offset);
        for (Elf64_Sym &sym : symbols) {
            symbol_names.emplace_back(str_tbl_start + sym.st_name);
        }
    } else {
        DEBUG_LOG("No symbol name string table found in sections, skipping.");
    }
    return EXIT_SUCCESS;
}

error_t ELF64File::init_header() {
    if (file_content.size() < sizeof(header)) {
        std::cerr << "The entered ELF-file's size is too small.\n";
        return ENOEXEC;
    }
    // We have to assume the header is conformant to the specified format
    std::memcpy(&header, file_content.data(), sizeof(header));
    return EXIT_SUCCESS;
}

std::optional<size_t> ELF64File::start_symbol() const {
    // find the symbol with a virtual address corresponding to the entry point defined in the elf header
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (symbols.at(i).st_value == header.e_entry) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::string> ELF64File::symbol_str_at_addr(uint64_t virt_addr) const {
    auto it = std::find_if(symbols.begin(), symbols.end(), [virt_addr](auto &sym) { return sym.st_value == virt_addr; });
    if (it != symbols.end()) {
        return symbol_names.at(std::distance(symbols.begin(), it));
    }
    return std::nullopt;
}
