#include "lifter/elf_file.h"

ELF64File::ELF64File(std::filesystem::path path) : file_path(std::move(path)) {
    DEBUG_LOG("Start reading ELF file.")
    read_file();
    init_header();

    if (!is_valid_elf_file()) {
        throw std::invalid_argument("Invalid ELF input file");
    }

    // It's possible for an elf-file to not contain sections, which is currently not supported.
    assert(header.e_shoff != 0);
    parse_sections();

    if (header.e_phoff != 0) {
        parse_program_headers();
    }

    parse_symbols();
    DEBUG_LOG("Done reading valid ELF file.");
}

bool ELF64File::is_valid_elf_file() const {
    // ELF file test
    if (header.e_ident[0] != ELFMAG0 ||
        header.e_ident[1] != ELFMAG1 ||
        header.e_ident[2] != ELFMAG2 ||
        header.e_ident[3] != ELFMAG3) {
        std::cout << "Invalid ELF file (wrong file identification byte(s)).\n";
        return false;
    }

    // 64-Bit test
    if (header.e_type != ELFCLASS64) {
        std::cout << "Invalid ELF file (only 64-bit ELF files are currently supported).\n";
        return false;
    }

    // Little-endian test
    if (file_content.at(EI_DATA) != ELFDATA2LSB) {
        std::cout << "Invalid ELF file (only little-endian ELF files are currently supported).\n";
        return false;
    }

    // RISC-V operating system test
    if (header.e_machine != EM_RISCV) {
        std::cout << "Invalid ELF file (only RISC-V ELF files are supported).\n";
        return false;
    }

    // System V ABI test
    if (file_content.at(EI_OSABI) != ELFOSABI_SYSV && file_content.at(EI_OSABI) != ELFOSABI_LINUX) {
        std::cout
                << "Invalid ELF file (only ELF files which were compiled for the Unix / System V ABI are currently supported).\n";
        return false;
    }

    // Static executable test
    if (header.e_type != ET_EXEC) {
        std::cout
                << "Invalid ELF file (only statically linked, executable ELF files are supported).\n";
        return false;
    }
    return true;
}

void ELF64File::read_file() {
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "Error during opening of file \"" << file_path.string() << "\": file does not exist.\n";
        throw std::invalid_argument("Invalid filepath.");
    }
    std::ifstream i_stream;

    try {
        i_stream.open(file_path, std::ios::in | std::ios::binary);
    } catch (const std::ifstream::failure &error) {
        std::cerr << "Error during opening of file \"" << file_path.string() << "\": " << error.what() << "\n";
        throw error;
    }

    if (i_stream.is_open()) {
        file_content = std::vector<uint8_t>((std::istreambuf_iterator<char>(i_stream)),
                                            std::istreambuf_iterator<char>());
        i_stream.close();
    } else {
        // do we even need this?
        std::cerr << "Couldn't open file \"" << file_path.string() << "\".\n";
        throw std::ios_base::failure("Unable to open file.");
    }
}

void ELF64File::parse_sections() {
    for (size_t i = 0; i < header.e_shnum; ++i) {
        Elf64_Shdr shdr;
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

        assert(section_headers.at(str_tbl_ind).sh_type == SHT_STRTAB);
        size_t str_table_offset = section_headers.at(str_tbl_ind).sh_offset;
        for (auto curr_hdr : section_headers) {
            std::vector<char> str;
            size_t j = 0;
            while (str.emplace_back(file_content[str_table_offset + curr_hdr.sh_name + j]) != '\0') {
                j++;
            }
            section_names.emplace_back(str.data());
        }
    } else {
        DEBUG_LOG("No section name string table declared in ELF file, skipping.");
    }
}

void ELF64File::parse_program_headers() {
    assert(!section_headers.empty());
    for (size_t i = 0; i < header.e_phnum; ++i) {
        Elf64_Phdr phdr;
        std::memcpy(&phdr, file_content.data() + header.e_phoff + (header.e_phentsize * i), sizeof(Elf64_Phdr));
        program_headers.push_back(phdr);

        segment_section_map.emplace_back();
        // rough approximation, for further reference: https://github.com/bminor/binutils-gdb/blob/master/include/elf/internal.h (line 323)
        for (auto sec : section_headers) {
            if (sec.sh_offset >= phdr.p_offset && sec.sh_offset + sec.sh_size <= phdr.p_offset + phdr.p_filesz) {
                segment_section_map.back().push_back(&sec);
            }
        }
    }
}

void ELF64File::parse_symbols() {
    assert(!section_headers.empty());

    size_t sym_tbl_i = SIZE_MAX;
    size_t sym_str_tbl_i = SIZE_MAX;
    for (size_t i = 0; i < section_headers.size(); i++) {
        if(section_headers.at(i).sh_type == SHT_SYMTAB) {
            sym_tbl_i = i;
        }
        if(section_headers.at(i).sh_type == SHT_STRTAB) {
            sym_str_tbl_i = i;
        }
        if (sym_tbl_i != SIZE_MAX && sym_str_tbl_i != SIZE_MAX) {
            break;
        }
    }
    // a symbol section should be present in the current file
    if (sym_tbl_i == SIZE_MAX) {
        std::cerr << "No symbol section was found in the ELF file.\n";
        throw std::invalid_argument("No symbol section found.");
    }
    Elf64_Shdr sym_tbl = section_headers.at(sym_tbl_i);

    for (size_t i = 0; i < sym_tbl.sh_size / sym_tbl.sh_entsize; ++i) {
        Elf64_Sym sym;
        std::memcpy(&sym, file_content.data() + sym_tbl.sh_offset + (sym_tbl.sh_entsize * i), sizeof(Elf64_Sym));
        symbols.push_back(sym);
    }

    if (sym_str_tbl_i != SIZE_MAX) {
        Elf64_Shdr sym_str_tbl = section_headers.at(sym_str_tbl_i);
        for (auto sym : symbols) {
            std::vector<char> str;
            size_t j = 0;
            while (str.emplace_back(file_content[sym_str_tbl.sh_offset + sym.st_name + j]) != '\0') {
                j++;
            }
            symbol_names.emplace_back(str.data());
        }
    } else {
        DEBUG_LOG("No symbol name string table found in sections, skipping.");
    }
}

void ELF64File::init_header() {
    std::memcpy(&header, file_content.data(), sizeof(header));
}

size_t ELF64File::start_symbol() const {
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (symbols.at(i).st_value == header.e_entry) {
            return i;
        }
    }
    throw std::invalid_argument("Can't find symbol at entry point address.");
}
