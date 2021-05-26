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

const char panic_str[] = "PANIC: ";

enum RV_SYSCALL_ID : uint32_t { RISCV_EXIT = 93 };

enum AMD64_SYSCALL_ID : uint32_t { AMD64_WRITE = 1, AMD64_EXIT = 60 };
} // namespace

extern "C" [[noreturn]] void panic(const char *err_msg);

extern "C" uint64_t syscall_impl(uint64_t id, uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    switch (id) {
    case RISCV_EXIT:
        return syscall1(AMD64_EXIT, arg0);
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

size_t strlen(const char *str) {
    size_t c = 0;
    while (*str++)
        ++c;
    return c;
}
} // namespace
