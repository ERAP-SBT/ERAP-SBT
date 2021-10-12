#pragma once

#include "generator/syscall_ids.h"

#include <cstddef>
#include <cstdint>
#include <immintrin.h>

namespace helper {

extern bool have_fma;

extern "C" {
/* provided by the compiler */
extern uint8_t *orig_binary_vaddr;
extern uint64_t phdr_off;
extern uint64_t phdr_num;
extern uint64_t phdr_size;

/* provided by the helper library */
uint64_t syscall_impl(uint64_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);
[[noreturn]] void panic(const char *err_msg);
uint8_t *copy_stack(uint8_t *stack, uint8_t *out_stack);
void resolve_dynamic_rounding(uint32_t dyn_rm);
}

/* Hashing helper for ijump target resolution */
struct HashTableTuple {
    uint64_t addr, target;
};

//extern "C" const HashTableTuple ijump_hash_table[];
extern "C" const uint64_t ijump_lookup_table_base;
extern "C" const uint64_t ijump_lookup_table[];
extern "C" const uint64_t ijump_lookup_table_end;

extern "C" const uint16_t ijump_hash_function_idxs[];
extern "C" const size_t ijump_hash_bucket_number;
extern "C" const size_t ijump_hash_table_size;

size_t calc_target(uint64_t addr);

// from https://github.com/aengelke/ria-jit/blob/master/src/runtime/emulateEcall.c
extern size_t syscall0(AMD64_SYSCALL_ID id);
extern size_t syscall1(AMD64_SYSCALL_ID id, size_t a1);
extern size_t syscall2(AMD64_SYSCALL_ID id, size_t a1, size_t a2);
extern size_t syscall3(AMD64_SYSCALL_ID id, size_t a1, size_t a2, size_t a3);
extern size_t syscall4(AMD64_SYSCALL_ID id, size_t a1, size_t a2, size_t a3, size_t a4);
extern size_t syscall5(AMD64_SYSCALL_ID id, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5);
extern size_t syscall6(AMD64_SYSCALL_ID id, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6);

/* Helpers similiar to stdlib */

constexpr size_t strlen(const char *str) {
    size_t c = 0;
    while (*str++)
        ++c;
    return c;
}

void memcpy(void *dst, const void *src, size_t count);

size_t write_stderr(const char *buf, size_t buf_len);
size_t puts(const char *str);

void utoa(uint64_t v, char *buf, unsigned int base, unsigned int num_digits);
void print_hex8(uint8_t byte);
void print_hex16(uint16_t byte);
void print_hex32(uint32_t byte);
void print_hex64(uint64_t byte);

namespace interpreter {

void interpreter_dump_perf_stats();
uint32_t evaluate_rounding_mode(uint32_t riscv_rounding_mode, const bool is_rm_field);
} // namespace interpreter

} // namespace helper
