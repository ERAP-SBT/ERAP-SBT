#include "stddef.h"

#include <cstdint>
// TODO: define these structs ourselves?
#include <sys/stat.h>

extern "C" {
extern uint8_t *orig_binary_vaddr;
extern uint64_t phdr_off;
extern uint64_t phdr_num;
extern uint64_t phdr_size;
}

namespace {

// from https://github.com/aengelke/ria-jit/blob/master/src/runtime/emulateEcall.c
[[maybe_unused]] size_t syscall0(int syscall_number);
[[maybe_unused]] size_t syscall1(int syscall_number, size_t a1);
[[maybe_unused]] size_t syscall2(int syscall_number, size_t a1, size_t a2);
[[maybe_unused]] size_t syscall3(int syscall_number, size_t a1, size_t a2, size_t a3);
[[maybe_unused]] size_t syscall4(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4);
[[maybe_unused]] size_t syscall5(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5);
[[maybe_unused]] size_t syscall6(int syscall_number, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6);

size_t strlen(const char *);
void memcpy(void *dst, const void *src, size_t count);
void itoa(char *str_addr, unsigned int num, unsigned int num_digits);

const char panic_str[] = "PANIC: ";

enum RV_SYSCALL_ID : uint32_t {
    RISCV_IOCTL = 29,
    RISCV_MKDIRAT = 34,
    RISCV_FTRUNCATE = 46,
    RISCV_FCHMODAT = 53,
    RISCV_OPENAT = 56,
    RISCV_CLOSE = 57,
    RISCV_LSEEK = 62,
    RISCV_READ = 63,
    RISCV_WRITE = 64,
    RISCV_READV = 65,
    RISCV_WRITEV = 66,
    RISCV_READ_LINK_AT = 78,
    RISCV_FSTATAT = 79,
    RISCV_FSTAT = 80,
    RISCV_UTIME_NS_AT = 88,
    RISCV_EXIT = 93,
    RISCV_EXIT_GROUP = 94,
    RISCV_SET_TID_ADDR = 96,
    RISCV_FUTEX = 98,
    RISCV_CLOCK_GET_TIME = 113,
    RISCV_UNAME = 160,
    RISCV_BRK = 214,
    RISCV_MUNMAP = 215,
    RISCV_MMAP = 222,
    RISCV_MPROTECT = 226,
    RISCV_MADVISE = 233
};

enum AMD64_SYSCALL_ID : uint32_t {
    AMD64_READ = 0,
    AMD64_WRITE = 1,
    AMD64_CLOSE = 3,
    AMD64_FSTAT = 5,
    AMD64_LSEEK = 8,
    AMD64_MMAP = 9,
    AMD64_MPROTECT = 10,
    AMD64_MUNMAP = 11,
    AMD64_BRK = 12,
    AMD64_IOCTL = 16,
    AMD64_READV = 19,
    AMD64_WRITEV = 20,
    AMD64_MADVISE = 28,
    AMD64_EXIT = 60,
    AMD64_UNAME = 63,
    AMD64_FTRUNCATE = 77,
    AMD64_FUTEX = 202,
    AMD64_SET_TID_ADDR = 218,
    AMD64_CLOCK_GET_TIME = 228,
    AMD64_EXIT_GROUP = 231,
    AMD64_OPENAT = 257,
    AMD64_MKDIRAT = 258,
    AMD64_NEWFSTATAT = 262,
    AMD64_READ_LINK_AT = 267,
    AMD64_FCHMODAT = 268,
    AMD64_UTIME_NS_AT = 280,
};

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

struct rv64_fstat_t {
    unsigned long st_dev;  /* Device.  */
    unsigned long st_ino;  /* File serial number.  */
    unsigned int st_mode;  /* File mode.  */
    unsigned int st_nlink; /* Link count.  */
    unsigned int st_uid;   /* User ID of the file's owner.  */
    unsigned int st_gid;   /* Group ID of the file's group. */
    unsigned long st_rdev; /* Device number, if device.  */
    unsigned long __pad1;
    long st_size;   /* Size of file, in bytes.  */
    int st_blksize; /* Optimal block size for I/O.  */
    int __pad2;
    long st_blocks; /* Number 512-byte blocks allocated. */
    long st_atim;   /* Time of last access.  */
    unsigned long st_atime_nsec;
    long st_mtim; /* Time of last modification.  */
    unsigned long st_mtime_nsec;
    long st_ctim; /* Time of last status change.  */
    unsigned long st_ctime_nsec;
    unsigned int __unused4;
    unsigned int __unused5;
};
} // namespace

extern "C" [[noreturn]] void panic(const char *err_msg);

extern "C" uint64_t syscall_impl(uint64_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch (id) {
    case RISCV_IOCTL:
        return syscall3(AMD64_IOCTL, arg0, arg1, arg2);
    case RISCV_MKDIRAT:
        return syscall3(AMD64_MKDIRAT, arg0, arg1, arg2);
    case RISCV_FTRUNCATE:
        return syscall2(AMD64_FTRUNCATE, arg0, arg1);
    case RISCV_FCHMODAT:
        return syscall2(AMD64_FCHMODAT, arg0, arg1);
    case RISCV_OPENAT:
        return syscall3(AMD64_OPENAT, arg0, arg1, arg2);
    case RISCV_CLOSE:
        return syscall1(AMD64_CLOSE, arg0);
    case RISCV_LSEEK:
        return syscall3(AMD64_LSEEK, arg0, arg1, arg2);
    case RISCV_READ:
        return syscall3(AMD64_READ, arg0, arg1, arg2);
    case RISCV_WRITE:
        return syscall3(AMD64_WRITE, arg0, arg1, arg2);
    case RISCV_READV:
        return syscall3(AMD64_READV, arg0, arg1, arg2);
    case RISCV_WRITEV:
        return syscall3(AMD64_WRITEV, arg0, arg1, arg2);
    case RISCV_READ_LINK_AT:
        return syscall4(AMD64_READ_LINK_AT, arg0, arg1, arg2, arg3);
    case RISCV_FSTATAT: {
        struct stat buf = {};
        const auto result = syscall4(AMD64_NEWFSTATAT, arg0, arg1, reinterpret_cast<uint64_t>(&buf), arg3);
        auto *r_stat = reinterpret_cast<rv64_fstat_t *>(arg2);
        r_stat->st_blksize = buf.st_blksize;
        r_stat->st_size = buf.st_size;
        r_stat->st_atim = buf.st_atim.tv_sec;
        r_stat->st_atime_nsec = buf.st_atim.tv_nsec;
        r_stat->st_blocks = buf.st_blocks;
        r_stat->st_ctim = buf.st_ctim.tv_sec;
        r_stat->st_ctime_nsec = buf.st_ctim.tv_nsec;
        r_stat->st_dev = buf.st_dev;
        r_stat->st_gid = buf.st_gid;
        r_stat->st_ino = buf.st_ino;
        r_stat->st_mode = buf.st_mode;
        r_stat->st_mtim = buf.st_mtim.tv_sec;
        r_stat->st_mtime_nsec = buf.st_mtim.tv_nsec;
        r_stat->st_nlink = buf.st_nlink;
        r_stat->st_rdev = buf.st_rdev;
        r_stat->st_uid = buf.st_uid;
        return result;
    }
    case RISCV_FSTAT: {
        struct stat buf = {};
        const auto result = syscall2(AMD64_FSTAT, arg0, reinterpret_cast<uint64_t>(&buf));
        auto *r_stat = reinterpret_cast<rv64_fstat_t *>(arg1);
        r_stat->st_blksize = buf.st_blksize;
        r_stat->st_size = buf.st_size;
        r_stat->st_atim = buf.st_atim.tv_sec;
        r_stat->st_atime_nsec = buf.st_atim.tv_nsec;
        r_stat->st_blocks = buf.st_blocks;
        r_stat->st_ctim = buf.st_ctim.tv_sec;
        r_stat->st_ctime_nsec = buf.st_ctim.tv_nsec;
        r_stat->st_dev = buf.st_dev;
        r_stat->st_gid = buf.st_gid;
        r_stat->st_ino = buf.st_ino;
        r_stat->st_mode = buf.st_mode;
        r_stat->st_mtim = buf.st_mtim.tv_sec;
        r_stat->st_mtime_nsec = buf.st_mtim.tv_nsec;
        r_stat->st_nlink = buf.st_nlink;
        r_stat->st_rdev = buf.st_rdev;
        r_stat->st_uid = buf.st_uid;
        return result;
    }
    case RISCV_UTIME_NS_AT:
        return syscall4(AMD64_UTIME_NS_AT, arg0, arg1, arg2, arg3);
    case RISCV_EXIT:
        return syscall1(AMD64_EXIT, arg0);
    case RISCV_EXIT_GROUP:
        return syscall1(AMD64_EXIT_GROUP, arg0);
    case RISCV_SET_TID_ADDR:
        return syscall1(AMD64_SET_TID_ADDR, arg0);
    case RISCV_FUTEX:
        return syscall6(AMD64_FUTEX, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_CLOCK_GET_TIME:
        return syscall2(AMD64_CLOCK_GET_TIME, arg0, arg1);
    case RISCV_UNAME:
        return syscall1(AMD64_UNAME, arg0);
    case RISCV_BRK:
        return syscall1(AMD64_BRK, arg0);
    case RISCV_MUNMAP:
        return syscall2(AMD64_MUNMAP, arg0, arg1);
    case RISCV_MMAP:
        return syscall6(AMD64_MMAP, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_MPROTECT:
        return syscall3(AMD64_MPROTECT, arg0, arg1, arg2);
    case RISCV_MADVISE:
        return syscall3(AMD64_MADVISE, arg0, arg1, arg2);
    default:
        break;
    }

    char syscall_str[31 + 8 + 1 + 1] = "Couldn't translate syscall ID: ";
    itoa(syscall_str + 31, id, 8);
    syscall_str[31 + 8] = '\n';
    syscall_str[sizeof(syscall_str) - 1] = '\0';
    panic(syscall_str);
}

extern "C" [[noreturn]] void panic(const char *err_msg) {
    static_assert(sizeof(size_t) == sizeof(const char *));
    syscall3(AMD64_WRITE, 2 /*stderr*/, reinterpret_cast<size_t>(panic_str), sizeof(panic_str));
    // in theory string length is known so maybe give it as an arg?
    syscall3(AMD64_WRITE, 2 /*stderr*/, reinterpret_cast<size_t>(err_msg), strlen(err_msg));
    syscall1(AMD64_EXIT, 1);
    __builtin_unreachable();
}

/**
 * @param stack x86 stack, as setup by the Operating System
 * @param out_stack RISC-V pseudo stack
 */
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
    for (auto *cur_auxv = auxv; cur_auxv->a_type != auxv_t::type::null; ++cur_auxv) {
        ++auxv_len;

        // modifying here because this loop already exists
        if (cur_auxv->a_type == auxv_t::type::phdr) {
            cur_auxv->p_ptr = (reinterpret_cast<uint8_t *>(&orig_binary_vaddr) + phdr_off);
        } else if (cur_auxv->a_type == auxv_t::type::phent) {
            cur_auxv->a_val = phdr_size;
        } else if (cur_auxv->a_type == auxv_t::type::phnum) {
            cur_auxv->a_val = phdr_num;
        }
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

void itoa(char *str_addr, unsigned int num, unsigned int num_digits) {
    for (int j = num_digits - 1; j >= 0; j--) {
        str_addr[j] = num % 10 + '0';
        num /= 10;
    }
}

} // namespace
