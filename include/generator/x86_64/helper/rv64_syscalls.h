#pragma once
#include "generator/syscall_ids.h"

#include <cstddef>
#include <cstdint>

namespace helper {

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

extern const SyscallInfo rv64_syscall_table[static_cast<size_t>(RISCV_SYSCALL_ID::SYSCALL_ID_MAX)];

} // namespace helper
