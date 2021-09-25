#include "generator/x86_64/helper/helper.h"

#include <cstdint>
#include <linux/errno.h>
#include <sys/signal.h>

#ifndef SA_RESTORER
#define SA_RESTORER 0x04000000
#endif

constexpr size_t REGISTER_COUNT = 66;

extern "C" uint64_t register_file[REGISTER_COUNT];

// Defined in wrapper.S
extern "C" void sh_signal_proxy_1(int sig);
extern "C" void sh_signal_proxy_3(int sig, siginfo_t *siginfo, void *ucontext);
extern "C" void sh_signal_restorer();

namespace helper::signal {

static struct {
    uint64_t handler_virtual_addresses[NSIG] = {0};

    uint64_t register_file[REGISTER_COUNT];

    bool is_in_signal{false};
    uint64_t rsp;
} signal_context;

extern "C" void sh_enter_signal(uint64_t sp) {
    memcpy8(signal_context.register_file, register_file, REGISTER_COUNT);
    signal_context.is_in_signal = true;
    signal_context.rsp = sp;
}

extern "C" uint64_t sh_exit_signal(uint64_t sp) {
    if (!signal_context.is_in_signal) {
        panic("sh_exit_signal called, but not currently in a signal");
    }
    memcpy8(register_file, signal_context.register_file, REGISTER_COUNT);
    signal_context.is_in_signal = false;
    if (sp == signal_context.rsp - 8) {
        return sp;
    } else if (sp == signal_context.rsp) {
        // If the signal restorer is reached through the ijump to the trampoline, the return address
        // is not yet popped from the stack
        return sp + 8;
    } else {
        panic("On signal return: Stack pointer misaligned, program likely corrupted");
    }
}

extern "C" uint64_t sh_lookup_signal_handler(int sig) {
    if (sig < 0 || sig >= NSIG) {
        panic("Got invalid signal");
    }
    return signal_context.handler_virtual_addresses[sig];
}

struct kernel_sigaction_rv64 {
    uint64_t handler;
    unsigned long flags;
    sigset_t mask;
};

struct kernel_sigaction_x86 {
    void *handler;
    unsigned long flags;
    void *restorer;
    sigset_t mask;
};

size_t handle_sigaction(int sig, const kernel_sigaction_rv64 *act, kernel_sigaction_rv64 *oact, size_t sigset_size) {
    if (sig < 0 || sig >= NSIG) {
        // Fail fast on invalid signal
        return -EINVAL;
    }

    if (act && (act->flags & SA_ONSTACK)) {
        // TODO support alternate stack
        return -EINVAL;
    }

    kernel_sigaction_x86 act_x86, oact_x86;

    if (act) {
        act_x86.flags = act->flags | SA_RESTORER;
        act_x86.mask = act->mask;
        act_x86.restorer = reinterpret_cast<void *>(&sh_signal_restorer);
        if (act_x86.flags & SA_SIGINFO) {
            act_x86.handler = reinterpret_cast<void *>(&sh_signal_proxy_3);
        } else {
            act_x86.handler = reinterpret_cast<void *>(&sh_signal_proxy_1);
        }
    }

    kernel_sigaction_x86 *act_ptr = act ? &act_x86 : nullptr, *oact_ptr = oact ? &oact_x86 : nullptr;
    size_t result = syscall4(AMD64_SYSCALL_ID::RT_SIGACTION, static_cast<size_t>(sig), reinterpret_cast<size_t>(act_ptr), reinterpret_cast<size_t>(oact_ptr), sigset_size);

    // Only continue on success
    if (static_cast<int64_t>(result) < 0)
        return result;

    signal_context.handler_virtual_addresses[static_cast<size_t>(sig)] = act->handler;

    if (oact) {
        oact->flags = oact_x86.flags & ~SA_RESTORER;
        oact->mask = oact_x86.mask;
        oact->handler = 0; // TODO
    }

    return result;
}

} // namespace helper::signal
