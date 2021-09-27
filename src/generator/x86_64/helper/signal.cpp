#include "generator/x86_64/helper/signal.h"

#include "generator/x86_64/helper/helper.h"

#include <cstdint>
#include <linux/errno.h>
#include <sys/signal.h>

#ifndef SA_RESTORER
#define SA_RESTORER 0x04000000
#endif

constexpr size_t REGISTER_COUNT = 66;

extern "C" uint64_t register_file[REGISTER_COUNT];

// Symbol defined in compiled code - do not read.
extern "C" uint64_t signal_trampoline_vaddr;
#define SIGNAL_TRAMPOLINE_VADDR reinterpret_cast<uint64_t>(&::signal_trampoline_vaddr)

// Defined in wrapper.S
extern "C" void sh_signal_proxy_1(int sig);
extern "C" void sh_signal_proxy_3(int sig, siginfo_t *siginfo, void *ucontext);
extern "C" void sh_signal_restorer();

// Defined in interpreter
extern "C" uint64_t unresolved_ijump_handler(uint64_t address);
extern "C" uint64_t ijump_lookup_for_addr(uint64_t address);

namespace helper::signal {

struct HandlerContext {
    uint64_t handler_virtual_address;
    bool use_alternate_stack;
};

static struct {
    HandlerContext sig_context[NSIG];

    uint64_t register_file[REGISTER_COUNT];

    uint64_t alternate_stack_ptr;
    uint64_t alternate_stack_size;

    bool is_in_signal;
    int current_signal;

    uint64_t rsp;
} g_signal_state;

extern "C" uint64_t sh_enter_signal(int sig, void *siginfo, void *ucontext, uint64_t sp) {
    if (sig < 0 || sig >= NSIG) {
        panic("Got invalid signal");
    }
    if (g_signal_state.is_in_signal) {
        panic("Concurrent signals not supported");
    }

    memcpy8(g_signal_state.register_file, register_file, REGISTER_COUNT);
    g_signal_state.current_signal = sig;
    g_signal_state.is_in_signal = true;
    g_signal_state.rsp = sp;

    // Set x1/ra (return address) to signal trampoline
    register_file[1] = SIGNAL_TRAMPOLINE_VADDR;

    // Set x10/a0 (first argument) to signal id
    register_file[10] = static_cast<uint64_t>(sig);

    // The assembly wrapper sets siginfo/ucontext to null if this is the one-argument variant

    // Set x11/a1 (second argument) to siginfo
    register_file[11] = reinterpret_cast<uint64_t>(siginfo);

    // Set x12/a2 (third argument) to ucontext
    register_file[12] = reinterpret_cast<uint64_t>(ucontext);

    auto &context = g_signal_state.sig_context[static_cast<size_t>(sig)];

    if (g_signal_state.alternate_stack_ptr != 0 && context.use_alternate_stack) {
        // Set x2/sp (stack pointer) to the alternate stack
        register_file[2] = g_signal_state.alternate_stack_ptr;
    }

    // Try to lookup signal handler address, or call interpreter if the block isn't found
    uint64_t handler_x86 = ijump_lookup_for_addr(context.handler_virtual_address);

    if (handler_x86 == 0) {
        handler_x86 = unresolved_ijump_handler(context.handler_virtual_address);
    }

    return handler_x86;
}

extern "C" uint64_t sh_exit_signal(uint64_t sp) {
    if (!g_signal_state.is_in_signal) {
        panic("sh_exit_signal called, but not currently in a signal");
    }

    memcpy8(register_file, g_signal_state.register_file, REGISTER_COUNT);
    g_signal_state.current_signal = -1;
    g_signal_state.is_in_signal = false;
    if (sp == g_signal_state.rsp + 8) {
        return sp;
    } else if (sp == g_signal_state.rsp) {
        // If the signal restorer is reached through the ijump to the trampoline, the return address
        // is not yet popped from the stack
        return sp + 8;
    } else {
        panic("On signal return: Stack pointer misaligned, program likely corrupted");
    }
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

    kernel_sigaction_x86 act_x86, oact_x86;

    if (act) {
        act_x86.flags = (act->flags & ~SA_ONSTACK) | SA_RESTORER;
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

    auto &context = g_signal_state.sig_context[static_cast<size_t>(sig)];

    if (oact) {
        oact->flags = oact_x86.flags & ~SA_RESTORER;
        oact->mask = oact_x86.mask;
        oact->handler = oact_x86.handler != nullptr ? context.handler_virtual_address : 0;
    }

    if (act) {
        context.handler_virtual_address = act->handler;
        context.use_alternate_stack = (act->flags & SA_ONSTACK) != 0;
    }

    return result;
}

struct kernel_sigaltstack {
    uint64_t sp;
    int flags;
    size_t size;
};

// Use system MINSIGSTKSZ for now
constexpr size_t RV64_MIN_SIGNAL_STACK_SIZE = MINSIGSTKSZ;

size_t handle_sigaltstack(const kernel_sigaltstack *ss, kernel_sigaltstack *oss) {
    if (oss != nullptr) {
        oss->sp = g_signal_state.alternate_stack_ptr;
        oss->size = g_signal_state.alternate_stack_size;

        oss->flags = 0;
        if (g_signal_state.is_in_signal && g_signal_state.alternate_stack_ptr != 0 && g_signal_state.sig_context[g_signal_state.current_signal].use_alternate_stack) {
            oss->flags |= SS_ONSTACK;
        }
        if (g_signal_state.alternate_stack_ptr == 0) {
            oss->flags |= SS_DISABLE;
        }
    }

    if (ss != nullptr) {
        if (ss->flags != 0) {
            // No SS_AUTODISARM support for now
            return -EINVAL;
        }
        if (ss->sp == 0 || ss->size < RV64_MIN_SIGNAL_STACK_SIZE) {
            return -ENOMEM;
        }
        if (g_signal_state.is_in_signal && g_signal_state.alternate_stack_ptr != 0) {
            return -EPERM;
        }

        g_signal_state.alternate_stack_ptr = ss->sp;
        g_signal_state.alternate_stack_size = ss->size;
    }

    return 0;
}

} // namespace helper::signal
