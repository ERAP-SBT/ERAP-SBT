#pragma once

#include <cstddef>
#include <cstdint>

namespace helper::signal {

struct kernel_sigaction_rv64;

size_t handle_sigaction(int sig, const kernel_sigaction_rv64 *act, kernel_sigaction_rv64 *oact, size_t sigset_size);

struct kernel_sigaltstack;

size_t handle_sigaltstack(const kernel_sigaltstack *ss, kernel_sigaltstack *oss);

} // namespace helper::signal
