#include "generator/x86_64/assembler.h"

#include "generator/x86_64/generator.h"

using namespace generator::x86_64;
using namespace ELFIO;

Assembler::Assembler(IR *ir, std::vector<uint8_t> &&binary_image) {
    this->ir = ir;
    instrs.resize(4096);
    instr_ptr = instrs.data();
    base_addr = ir->base_addr;

    elf_file = std::make_unique<ELFIO::elfio>();
    elf_file->create(ELFCLASS64, ELFDATA2LSB);
    elf_file->set_type(ET_REL);
    elf_file->set_os_abi(ELFOSABI_NONE);
    elf_file->set_machine(EM_X86_64);

    auto *bin_sec = elf_file->sections.add(".orig_binary");
    bin_sec->set_type(SHT_PROGBITS);
    bin_sec->set_flags(SHF_ALLOC | SHF_WRITE);
    binary_image.resize(ir->load_size);
    bin_sec->set_data(reinterpret_cast<char *>(binary_image.data()), binary_image.size());
    bin_sec->set_addr_align(0x10);
    orig_binary_sec_idx = bin_sec->get_index();

    // calculate static addresses to be used in assembly step
    bss_addr = (ir->base_addr + ir->load_size + 0x1000 - 1) & 0xFFFFFFFF'FFFFF000;
    statics_addr = bss_addr;
    param_pass_addr = (statics_addr + ir->statics.size() * 8 + 15) & 0xFFFFFFFF'FFFFFFF0;
    init_stack_ptr = param_pass_addr + 128;
    stack_addr = init_stack_ptr + 16;
    ijump_table_addr = (stack_addr + (2 * 1024 * 1024) + 0x1000 - 1) & 0xFFFFFFFF'FFFFF000; // 2MB size for stack
    stack_end_addr = ijump_table_addr - 8;
    ijump_table_size = (((ir->virt_bb_end_addr - ir->virt_bb_start_addr) * sizeof(uint64_t)) / 2 + 15) & 0xFFFFFFFF'FFFFFFF0;
    text_addr = (ijump_table_addr + ijump_table_size + 0x1000 - 1) & 0xFFFFFFFF'FFFFF000;

    // TODO: ids can be bigger than the vector size...
    bb_offsets.resize(ir->basic_blocks.size());
}

uint8_t **Assembler::cur_instr_ptr() {
    if (&*instrs.end() - instr_ptr < 15) {
        const auto cur_off = instr_ptr - instrs.data();
        // TODO: need bigger size
        instrs.resize(instrs.size() + 4096);
        instr_ptr = instrs.data() + cur_off;
    }
    return &instr_ptr;
}

void Assembler::start_new_bb(size_t id) { bb_offsets[id] = instr_ptr - instrs.data(); }

void Assembler::add_jmp_to_bb(size_t bb_id) {
    // only support +-2gb jumps
    bb_jumps.emplace_back(instr_ptr - instrs.data(), bb_id);
    fe_enc64(cur_instr_ptr(), FE_JMP | FE_JMPL, (intptr_t)instrs.data());
}

void Assembler::add_short_jmp(uint64_t mnem) {
    assert(cur_short_jmp_off == 0);
    cur_short_jmp_off = instr_ptr - instrs.data();
    cur_short_jmp_mnem = mnem;
    fe_enc64(cur_instr_ptr(), mnem | FE_JMPL, (intptr_t)instrs.data());
}

void Assembler::resolve_short_jmp() {
    auto *jmp_ptr = instrs.data() + cur_short_jmp_off;
    fe_enc64(&jmp_ptr, cur_short_jmp_mnem | FE_JMPL, (intptr_t)instr_ptr);
    cur_short_jmp_off = 0;
}

void Assembler::add_syscall() {
    syscall_calls.emplace_back(instr_ptr - instrs.data());
    fe_enc64(cur_instr_ptr(), FE_CALL, (intptr_t)instrs.data());
}

void Assembler::add_panic(size_t err_idx) {
    panic_calls.emplace_back(instr_ptr - instrs.data(), err_idx);
    fe_enc64(cur_instr_ptr(), FE_MOV64ri, FE_DI, 0);
    fe_enc64(cur_instr_ptr(), FE_JMP | FE_JMPL, (intptr_t)instrs.data());
}

void Assembler::load_static_in_reg(size_t idx, int64_t reg, Type type) {
    const auto static_addr = statics_addr + idx * 8;
    const auto off = -(((instr_ptr - instrs.data()) + text_addr) - static_addr);
    // scale must be 0 if idx == 0
    fe_enc64(cur_instr_ptr(), opcode(type, FE_MOV64rm, FE_MOV32rm, FE_MOV16rm, FE_MOV8rm), reg, FE_MEM(FE_IP, 0, 0, off));
}

void Assembler::save_static_from_reg(size_t idx, int64_t reg, Type type) {
    const auto static_addr = statics_addr + idx * 8;
    const auto off = -(((instr_ptr - instrs.data()) + text_addr) - static_addr);
    fe_enc64(cur_instr_ptr(), opcode(type, FE_MOV64mr, FE_MOV32mr, FE_MOV16mr, FE_MOV8mr), FE_MEM(FE_IP, 0, 0, off), reg);
}

void Assembler::load_binary_rel_val(int64_t reg, int64_t imm) {
    auto off = ir->base_addr - (text_addr + (instr_ptr - instrs.data())) + imm;
    fe_enc64(cur_instr_ptr(), FE_LEA64rm, reg, FE_MEM(FE_IP, 0, 0, off));
}

void Assembler::finish(std::string output_filename, Generator *gen) {
    for (const auto &[offset, bb_id] : bb_jumps) {
        auto *instr_ptr = instrs.data() + offset;
        fe_enc64(&instr_ptr, FE_JMP | FE_JMPL, (intptr_t)(instrs.data() + bb_offsets[bb_id]));
    }
    bb_jumps = {};

    // setup symbol section
    auto *str_sec = elf_file->sections.add(".strtab");
    str_sec->set_type(SHT_STRTAB);
    str_sec->set_addr_align(1);
    auto str_writer = string_section_accessor{str_sec};

    auto *sym_sec = elf_file->sections.add(".symtab");
    sym_sec->set_type(SHT_SYMTAB);
    sym_sec->set_info(2);
    sym_sec->set_link(str_sec->get_index());
    sym_sec->set_addr_align(4);
    sym_sec->set_entry_size(elf_file->get_default_entry_size(SHT_SYMTAB));
    auto sym_writer = symbol_section_accessor{*elf_file, sym_sec};

    const auto syscall_sym = sym_writer.add_symbol(str_writer, "syscall_impl", 0, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_UNDEF);
    const auto panic_sym = sym_writer.add_symbol(str_writer, "panic", 0, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_UNDEF);
    const auto copy_stack_sym = sym_writer.add_symbol(str_writer, "copy_stack", 0, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_UNDEF);

    // create start func
    auto start_off = (instr_ptr - instrs.data());
    // mov r12, offset param_passing
    fe_enc64(cur_instr_ptr(), FE_MOV64ri, FE_R12, param_pass_addr);
    // mov rdi, rsp
    fe_enc64(cur_instr_ptr(), FE_MOV64rr, FE_DI, FE_SP);
    // mov rsi, offset stack_end
    fe_enc64(cur_instr_ptr(), FE_MOV64ri, FE_SI, stack_end_addr);
    auto copy_stack_off = (instr_ptr - instrs.data());
    // call copy_stack
    fe_enc64(cur_instr_ptr(), FE_CALL, (intptr_t)instrs.data());
    // mov [init_stack_ptr], rax
    fe_enc64(cur_instr_ptr(), FE_MOV64ar, init_stack_ptr, FE_AX);
    // jmp entry_block
    fe_enc64(cur_instr_ptr(), FE_JMP, (intptr_t)(instrs.data() + bb_offsets[ir->entry_block]));

    auto *text_sec = elf_file->sections.add(".ttext");
    text_sec->set_type(SHT_PROGBITS);
    text_sec->set_flags(SHF_ALLOC | SHF_EXECINSTR);
    text_sec->set_addr_align(1);
    text_sec->set_size(instrs.size());

    auto *reloc_sec = elf_file->sections.add(".rela.ttext");
    reloc_sec->set_type(SHT_RELA);
    reloc_sec->set_flags(SHF_INFO_LINK);
    reloc_sec->set_info(text_sec->get_index());
    reloc_sec->set_link(sym_sec->get_index());
    reloc_sec->set_addr_align(8);
    reloc_sec->set_entry_size(elf_file->get_default_entry_size(SHT_RELA));
    auto relocations = relocation_section_accessor{*elf_file, reloc_sec};
    relocations.add_entry(copy_stack_off + 1, copy_stack_sym, R_X86_64_32S, -text_addr);
    for (const auto &offset : syscall_calls) {
        // call syscall_impl = E8 off
        relocations.add_entry(offset + 1, syscall_sym, R_X86_64_32S, -text_addr);
    }
    syscall_calls = {};

    for (size_t i = 0; i < bb_offsets.size(); ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "bb%zu", i);
        sym_writer.add_symbol(str_writer, buf, bb_offsets[i], 0, STB_LOCAL, STT_FUNC, 0, text_sec->get_index());
    }
    sym_writer.add_symbol(str_writer, "_start", start_off, 0, STB_GLOBAL, STT_NOTYPE, 0, text_sec->get_index());

    // bss has stack, param_passing and the statics
    auto *bss_sec = elf_file->sections.add(".tbss");
    bss_sec->set_type(SHT_NOBITS);
    bss_sec->set_flags(SHF_ALLOC | SHF_WRITE);
    bss_sec->set_size(ijump_table_addr - bss_addr);
    bss_sec->set_addr_align(0x10);

    auto *ijump_sec = elf_file->sections.add(".ijumps");
    ijump_sec->set_type(SHT_PROGBITS);
    ijump_sec->set_flags(SHF_ALLOC);
    ijump_sec->set_addr_align(0x10);

    std::vector<uint64_t> ijump_table{};
    ijump_table.reserve((ir->virt_bb_end_addr - ir->virt_bb_start_addr) / 2);
    for (uint64_t i = ir->virt_bb_start_addr; i < ir->virt_bb_end_addr; i += 2) {
        const auto bb = ir->bb_at_addr(i);
        if (bb != nullptr && bb->virt_start_addr == i) {
            ijump_table.emplace_back(bb_offsets[bb->id] + text_addr);
        } else {
            ijump_table.emplace_back(0);
        }
    }
    ijump_sec->set_data(reinterpret_cast<char *>(ijump_table.data()), ijump_table.size() * sizeof(uint64_t));
    ijump_table = {};

    // rodata contains the phdr stuff and error messages
    const auto rodata_addr = (text_addr + text_sec->get_size() + 0x1000 - 1) & 0xFFFFFFFF'FFFFF000;
    auto *rodata_sec = elf_file->sections.add(".rodata");
    rodata_sec->set_type(SHT_PROGBITS);
    rodata_sec->set_flags(SHF_ALLOC);
    rodata_sec->set_addr_align(8);
    rodata_sec->append_data(reinterpret_cast<char *>(&ir->phdr_off), sizeof(uint64_t));
    rodata_sec->append_data(reinterpret_cast<char *>(&ir->phdr_off), sizeof(uint64_t));
    rodata_sec->append_data(reinterpret_cast<char *>(&ir->phdr_off), sizeof(uint64_t));

    char buf[256];
    size_t off = 24;
    std::vector<size_t> err_offsets;
    err_offsets.reserve(gen->err_msgs.size());
    for (auto &err_msg : gen->err_msgs) {
        auto len = 0;
        switch (err_msg.first) {
        case Generator::ErrType::unreachable:
            len = snprintf(buf, sizeof(buf), "Reached unreachable code in block %zu\n", err_msg.second->id);
            break;
        case Generator::ErrType::unresolved_ijump:
            len = snprintf(buf, sizeof(buf), "Reached unresolved indirect jump in block%zu\n", err_msg.second->id);
            break;
        }
        err_offsets.emplace_back(off);
        off += len + 1;
        rodata_sec->append_data(buf, len + 1);
    }

    for (auto &call : panic_calls) {
        auto *cur = instrs.data() + call.first;
        fe_enc64(&cur, FE_MOV64ri, FE_DI, rodata_addr + err_offsets[call.second]);
        // jmp panic = E9 off
        auto offset = (cur - instrs.data());
        relocations.add_entry(offset + 1, panic_sym, R_X86_64_32S, -text_addr);
    }

    text_sec->set_data(reinterpret_cast<const char *>(instrs.data()), instrs.size());
    instrs = {};

    // symbols for the linker
    sym_writer.add_symbol(str_writer, "orig_binary_vaddr", ir->base_addr, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_ABS);
    sym_writer.add_symbol(str_writer, "orig_binary_size", ir->load_size, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_ABS);
    sym_writer.add_symbol(str_writer, "ttext_start", text_addr, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_ABS);
    sym_writer.add_symbol(str_writer, "tbss_start", bss_addr, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_ABS);
    sym_writer.add_symbol(str_writer, "ijump_table_start", ijump_table_addr, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_ABS);
    sym_writer.add_symbol(str_writer, "orig_binary", 0, ir->load_size, STB_LOCAL, STT_OBJECT, 0, orig_binary_sec_idx);
    sym_writer.add_symbol(str_writer, "rodata_start", rodata_addr, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_ABS);
    sym_writer.add_symbol(str_writer, "helper_start", (rodata_addr + rodata_sec->get_size() + 0x1000 - 1) & 0xFFFFFFFF'FFFFF000, 0, STB_GLOBAL, STT_NOTYPE, 0, SHN_ABS);
    sym_writer.add_symbol(str_writer, "phdr_off", 0, 0, STB_GLOBAL, STT_NOTYPE, 0, rodata_sec->get_index());
    sym_writer.add_symbol(str_writer, "phdr_size", 8, 0, STB_GLOBAL, STT_NOTYPE, 0, rodata_sec->get_index());
    sym_writer.add_symbol(str_writer, "phdr_num", 16, 0, STB_GLOBAL, STT_NOTYPE, 0, rodata_sec->get_index());

    // debug info
    sym_writer.add_symbol(str_writer, "init_stack_ptr", init_stack_ptr - bss_addr, sizeof(uint64_t), STB_LOCAL, STT_OBJECT, bss_sec->get_index());
    sym_writer.add_symbol(str_writer, "trans_stack", stack_addr - bss_addr, stack_end_addr - stack_addr, STB_LOCAL, STT_OBJECT, 0, bss_sec->get_index());

    for (size_t i = 0; i < ir->statics.size(); ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "s%zu", i);
        sym_writer.add_symbol(str_writer, buf, (statics_addr + i * sizeof(uint64_t)) - bss_addr, sizeof(uint64_t), STB_LOCAL, STT_NOTYPE, 0, bss_sec->get_index());
    }

    // fixup symbol order otherwise we can't link properly
    sym_writer.arrange_local_symbols([&relocations](auto first, auto second) { relocations.swap_symbols(first, second); });

    elf_file->save(output_filename);
}