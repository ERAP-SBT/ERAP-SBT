#pragma once
#include "generator/syscall_ids.h"

#include <cstdint>

enum class SyscallAction : uint8_t {
    passthrough,
    unimplemented,
    handle,
    succeed,
};

struct SyscallInfo {
    SyscallAction action;
    uint8_t param_count;
    AMD64_SYSCALL_ID translated_id;
};

extern const SyscallInfo rv64_syscall_table[RISCV_SYSCALL_ID_MAX + 1];