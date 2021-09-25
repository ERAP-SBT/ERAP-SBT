#pragma once

#include <cstddef>

namespace helper::signal {

struct kernel_sigaction_rv64;

size_t handle_sigaction(int sig, const kernel_sigaction_rv64 *act, kernel_sigaction_rv64 *oact, size_t sigset_size);

} // namespace helper::signal
