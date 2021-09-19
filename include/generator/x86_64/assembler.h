#pragma once
#include <elfio/elfio.hpp>
#include <fadec-enc.h>
#include <ir/ir.h>

namespace generator::x86_64 {
inline uint64_t opcode(Type type, uint64_t op64, uint64_t op32, uint64_t op16, uint64_t op8) {
    switch (type) {
    case Type::imm:
    case Type::i64:
        return op64;
    case Type::i32:
        return op32;
    case Type::i16:
        return op16;
    case Type::i8:
        return op8;
    default:
        assert(0);
    }
}

inline uint64_t opcode(Type type, uint64_t op64, uint64_t op32, uint64_t op16) {
    switch (type) {
    case Type::imm:
    case Type::i64:
        return op64;
    case Type::i32:
        return op32;
    case Type::i16:
        return op16;
    case Type::i8:
    default:
        assert(0);
    }
}

struct Generator;

struct Assembler {

    IR *ir;
    std::vector<uint8_t> instrs;
    uint8_t *instr_ptr = nullptr;
    size_t ijump_table_size = 0, ijump_table_addr = 0;
    size_t base_addr = 0; // this is also the addr at which the original binary is loaded
    size_t statics_addr = 0;
    size_t stack_addr = 0;
    size_t param_pass_addr = 0;
    size_t orig_binary_sec_idx = 0;
    // pair<offset in instrs vector, bb id to jump to>
    std::vector<std::pair<size_t, size_t>> bb_jumps{};
    // pair<offset in instrs vector, err msg idx>
    std::vector<std::pair<size_t, size_t>> panic_calls{};
    std::vector<size_t> syscall_calls{};
    std::vector<size_t> bb_offsets{}; // -1 = invalid
    std::unique_ptr<ELFIO::elfio> elf_file = nullptr;

    size_t stack_end_addr = 0;
    size_t init_stack_ptr = 0;
    size_t bss_addr = 0, text_addr = 0;
    size_t cur_short_jmp_off = 0;
    uint64_t cur_short_jmp_mnem = 0;

    Assembler() {}
    Assembler(IR *ir, std::vector<uint8_t> &&binary_image);

    uint8_t **cur_instr_ptr();
    void start_new_bb(size_t id);
    void add_jmp_to_bb(size_t bb_id);
    void add_short_jmp(uint64_t mnem);
    void resolve_short_jmp();
    void add_syscall();
    void add_panic(size_t err_msg_idx);
    void load_static_in_reg(size_t idx, int64_t reg, Type type);
    void save_static_from_reg(size_t idx, int64_t reg, Type type);
    void load_binary_rel_val(int64_t reg, int64_t imm);
    void finish(std::string output_filename, Generator *gen);
};
} // namespace generator::x86_64