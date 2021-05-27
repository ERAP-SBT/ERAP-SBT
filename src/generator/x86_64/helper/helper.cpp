#include <cstdint>

// otherwise gcc complains about size_t for some reason
using size_t = std::size_t;

namespace {
// from https://github.com/aengelke/ria-jit/blob/master/src/runtime/emulateEcall.c
size_t syscall0(int syscall_number);
size_t syscall1(int syscall_number, size_t a1);
size_t syscall2(int syscall_number, size_t a1, size_t a2);
size_t syscall3(int syscall_number, size_t a1, size_t a2, size_t a3);
size_t syscall4(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4);
size_t syscall5(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5);
size_t syscall6(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6);

size_t strlen(const char *);
void memcpy(void *dst, const void *src, size_t count);

const char panic_str[] = "PANIC: ";

enum RV_SYSCALL_ID : uint32_t { RISCV_READ = 63, RISCV_WRITE = 64, RISCV_EXIT = 93 };

enum AMD64_SYSCALL_ID : uint32_t { AMD64_READ = 0, AMD64_WRITE = 1, AMD64_EXIT = 60 };

struct auxv_t {
    // see https://fossies.org/dox/Checker-0.9.9.1/gcc-startup_8c_source.html#l00042
    // see also https://refspecs.linuxfoundation.org/ELF/zSeries/lzsabi0_zSeries/x895.html
    enum class type : size_t {
        null = 0,
        ignore = 1,
        exec_cfd = 2,
        phdr = 3,
        phent = 4,
        phnum = 5,
        pagesz = 6,
        base = 7,
        flags = 8,
        entry = 9,
        not_elf = 10,
        uid = 11,
        euid = 12,
        guid = 13,
        egid = 14
    };

    type a_type;
    union {
        size_t a_val;
        void *p_ptr;
        void (*a_fcn)(void); // this seems unused
    };
};
} // namespace

extern "C" [[noreturn]] void panic(const char *err_msg);

extern "C" uint64_t syscall_impl(uint64_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    switch (id) {
    case RISCV_READ:
        return syscall3(AMD64_READ, arg0, arg1, arg2);
    case RISCV_WRITE:
        return syscall3(AMD64_WRITE, arg0, arg1, arg2);
    case RISCV_EXIT:
        return syscall1(AMD64_EXIT, arg0);
    default:
        break;
    }

    panic("Couldn't translate syscall ID\n");
}

extern "C" [[noreturn]] void panic(const char *err_msg) {
    static_assert(sizeof(size_t) == sizeof(const char *));
    syscall3(AMD64_WRITE, 2 /*stderr*/, reinterpret_cast<size_t>(panic_str), sizeof(panic_str));
    // in theory string length is known so maybe give it as an arg?
    syscall3(AMD64_WRITE, 2 /*stderr*/, reinterpret_cast<size_t>(err_msg), strlen(err_msg));
    syscall1(AMD64_EXIT, 1);
}

extern "C" uint8_t *copy_stack(uint8_t *stack, uint8_t *out_stack) {
    /*
     * stack looks like this:
     * *data*
     * ///
     * null aux vec (8 bytes)
     * aux vec entries (n * 16 bytes)
     * 0 (8 bytes)
     * env str ptrs (n * 8 bytes)
     * 0 (8 bytes)
     * arg str ptrs (argc * 8 bytes)
     * argc (8 bytes) <- stack
     */
    // TODO: this copies all of the stack including the strings but you could probably get away with just copying the pointers
    const auto argc = *reinterpret_cast<size_t *>(stack);
    auto *argv = reinterpret_cast<char **>(reinterpret_cast<size_t>(stack + 8));
    auto *envp = argv + argc + 1;
    auxv_t *auxv = nullptr;
    {
        auto *ptr = envp;
        while (*ptr++)
            ;
        auxv = reinterpret_cast<auxv_t *>(ptr);
    }

    auto *orig_out_stack = out_stack;
    // copy all env strs first, then all arg strs
    for (const auto *cur_env = reinterpret_cast<char **>(auxv) - 2; cur_env >= envp; --cur_env) {
        const auto len = strlen(*cur_env) + 1;
        out_stack -= (len + 7) & ~7; // align to 8 byte
        memcpy(out_stack, *cur_env, len);
    }

    for (size_t i = argc; i > 0; --i) {
        const auto len = strlen(argv[i - 1]) + 1;
        out_stack -= (len + 7) & ~7; // align to 8 byte
        memcpy(out_stack, argv[i - 1], len);
    }

    size_t auxv_len = 0;
    for (const auto *cur_auxv = auxv; cur_auxv->a_type != auxv_t::type::null; ++cur_auxv) {
        ++auxv_len;
    }

    auxv_len = 8 + auxv_len * sizeof(auxv_t);
    out_stack -= auxv_len;
    memcpy(out_stack, auxv, auxv_len);

    out_stack -= 8;
    *reinterpret_cast<size_t *>(out_stack) = 0;
    for (const auto *cur_env = reinterpret_cast<char **>(auxv) - 2; cur_env >= envp; --cur_env) {
        const auto len = strlen(*cur_env) + 1;
        orig_out_stack -= (len + 7) & ~7; // align to 8 byte
        out_stack -= 8;
        *reinterpret_cast<uint8_t **>(out_stack) = orig_out_stack;
    }

    out_stack -= 8;
    *reinterpret_cast<size_t *>(out_stack) = 0;
    for (size_t i = argc; i > 0; --i) {
        const auto len = strlen(argv[i - 1]) + 1;
        orig_out_stack -= (len + 7) & ~7; // align to 8 byte
        out_stack -= 8;
        *reinterpret_cast<uint8_t **>(out_stack) = orig_out_stack;
    }

    out_stack -= 8;
    *reinterpret_cast<size_t *>(out_stack) = argc;
    return out_stack;
}

namespace {
// from https://github.com/aengelke/ria-jit/blob/master/src/runtime/emulateEcall.c
size_t syscall0(int syscall_number) {
    size_t retval = syscall_number;
    __asm__ volatile("syscall" : "+a"(retval) : : "memory", "rcx", "r11");
    return retval;
}

size_t syscall1(int syscall_number, size_t a1) {
    size_t retval = syscall_number;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall2(int syscall_number, size_t a1, size_t a2) {
    size_t retval = syscall_number;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall3(int syscall_number, size_t a1, size_t a2, size_t a3) {
    size_t retval = syscall_number;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2), "d"(a3) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall4(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4) {
    size_t retval = syscall_number;
    register size_t r10 __asm__("r10") = a4;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall5(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5) {
    size_t retval = syscall_number;
    register size_t r10 __asm__("r10") = a4;
    register size_t r8 __asm__("r8") = a5;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall6(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6) {
    size_t retval = syscall_number;
    register size_t r8 __asm__("r8") = a5;
    register size_t r9 __asm__("r9") = a6;
    register size_t r10 __asm__("r10") = a4;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2), "d"(a3), "r"(r8), "r"(r9), "r"(r10) : "memory", "rcx", "r11");
    return retval;
}

// need to implement ourselves without stdlib
size_t strlen(const char *str) {
    size_t c = 0;
    while (*str++)
        ++c;
    return c;
}

void memcpy(void *dst, const void *src, size_t count) {
    auto *dst_ptr = static_cast<uint8_t *>(dst);
    const auto *src_ptr = static_cast<const uint8_t *>(src);

    while (count > 0) {
        *dst_ptr++ = *src_ptr++;
        count--;
    }
}
} // namespace
