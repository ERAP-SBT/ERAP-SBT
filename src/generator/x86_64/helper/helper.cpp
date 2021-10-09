#include "generator/x86_64/helper/helper.h"

#include "generator/syscall_ids.h"
#include "generator/x86_64/helper/rv64_syscalls.h"

#include <cstddef>
#include <cstdint>
#include <elf.h>
#include <linux/errno.h>
#include <sys/epoll.h>
#include <sys/stat.h>

namespace helper {

/* dump interpreter statistics at exit */
#define INTERPRETER_DUMP_PERF_STATS_AT_EXIT false

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

// on riscv this struct isn't packed
struct rv64_epoll_event_t {
    uint32_t events;
    uint32_t _pad;
    epoll_data_t data;
};

// TODO: make a bitmap which syscalls are passthrough, which are not implemented
extern "C" uint64_t syscall_impl(uint64_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    if (id <= static_cast<uint64_t>(RISCV_SYSCALL_ID::SYSCALL_ID_MAX)) {
        const auto &info = rv64_syscall_table[id];
        if (info.action == SyscallAction::succeed) {
            return 0;
        } else if (info.action == SyscallAction::unimplemented) {
            return -ENOSYS;
        } else if (info.action == SyscallAction::passthrough) {
            // just assume 6?
            switch (info.param_count) {
            case 0:
                return syscall0(info.translated_id);
            case 1:
                return syscall1(info.translated_id, arg0);
            case 2:
                return syscall2(info.translated_id, arg0, arg1);
            case 3:
                return syscall3(info.translated_id, arg0, arg1, arg2);
            case 4:
                return syscall4(info.translated_id, arg0, arg1, arg2, arg3);
            case 5:
                return syscall5(info.translated_id, arg0, arg1, arg2, arg3, arg4);
            case 6:
                return syscall6(info.translated_id, arg0, arg1, arg2, arg3, arg4, arg5);
            default:
                break;
            }
        } else if (info.action == SyscallAction::handle) {
            switch (static_cast<RISCV_SYSCALL_ID>(id)) {
            case RISCV_SYSCALL_ID::EXIT:
            case RISCV_SYSCALL_ID::EXIT_GROUP: {
#if INTERPRETER_DUMP_PERF_STATS_AT_EXIT
                helper::interpreter::interpreter_dump_perf_stats();
#endif
                return syscall1(info.translated_id, arg0);
            }
            case RISCV_SYSCALL_ID::EPOLL_CTL: {
                struct epoll_event event;
                auto *rv64_event = reinterpret_cast<rv64_epoll_event_t *>(arg3);
                if (rv64_event) {
                    // can be null on CTL_DELETE
                    event.data = rv64_event->data;
                    event.events = rv64_event->events;
                }
                const auto res = syscall4(AMD64_SYSCALL_ID::EPOLL_CTL, arg0, arg1, arg2, reinterpret_cast<size_t>(&event));
                return res;
            }
            case RISCV_SYSCALL_ID::EPOLL_PWAIT: {
                struct epoll_event event {};
                auto *rv64_event = reinterpret_cast<rv64_epoll_event_t *>(arg1);
                const auto res = syscall6(AMD64_SYSCALL_ID::EPOLL_PWAIT, arg0, reinterpret_cast<size_t>(&event), arg2, arg3, arg4, arg5);
                rv64_event->data = event.data;
                rv64_event->events = event.events;
                return res;
            }
            case RISCV_SYSCALL_ID::FSTATAT: {
                struct stat buf {};
                const auto result = syscall4(AMD64_SYSCALL_ID::NEWFSTATAT, arg0, arg1, reinterpret_cast<uint64_t>(&buf), arg3);
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
            case RISCV_SYSCALL_ID::FSTAT: {
                struct stat buf {};
                const auto result = syscall2(AMD64_SYSCALL_ID::FSTAT, arg0, reinterpret_cast<uint64_t>(&buf));
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
            case RISCV_SYSCALL_ID::EPOLL_PWAIT2: {
                struct epoll_event event {};
                auto *rv64_event = reinterpret_cast<rv64_epoll_event_t *>(arg1);
                const auto res = syscall6(AMD64_SYSCALL_ID::EPOLL_PWAIT2, arg0, reinterpret_cast<size_t>(&event), arg2, arg3, arg4, arg5);
                rv64_event->data = event.data;
                rv64_event->events = event.events;
                return res;
            }
            case RISCV_SYSCALL_ID::CLONE: {
                // RISC-V uses a backwards variant, where arguments 4 and 5 (child_tidptr/tls) are swapped
                return syscall5(AMD64_SYSCALL_ID::CLONE, arg0, arg1, arg2, arg4, arg3);
            }
            default:
                break;
            }
        }
    }

    puts("Couldn't translate syscall ID: ");
    print_hex64(id);
    panic("Couldn't translate syscall ID");
}

extern "C" [[noreturn]] void panic(const char *err_msg) {
    puts("PANIC: ");
    // in theory string length is known so maybe give it as an arg?
    puts(err_msg);
    puts("\n");

    syscall1(AMD64_SYSCALL_ID::EXIT, 1);
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
        } else if (cur_auxv->a_type == static_cast<auxv_t::type>(AT_SYSINFO) || cur_auxv->a_type == static_cast<auxv_t::type>(AT_SYSINFO_EHDR)) {
            cur_auxv->a_val = 0;
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

// from https://github.com/aengelke/ria-jit/blob/master/src/runtime/emulateEcall.c
size_t syscall0(AMD64_SYSCALL_ID id) {
    auto retval = static_cast<size_t>(id);
    __asm__ volatile("syscall" : "+a"(retval) : : "memory", "rcx", "r11");
    return retval;
}

size_t syscall1(AMD64_SYSCALL_ID id, size_t a1) {
    auto retval = static_cast<size_t>(id);
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall2(AMD64_SYSCALL_ID id, size_t a1, size_t a2) {
    auto retval = static_cast<size_t>(id);
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall3(AMD64_SYSCALL_ID id, size_t a1, size_t a2, size_t a3) {
    auto retval = static_cast<size_t>(id);
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2), "d"(a3) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall4(AMD64_SYSCALL_ID id, size_t a1, size_t a2, size_t a3, size_t a4) {
    auto retval = static_cast<size_t>(id);
    register size_t r10 __asm__("r10") = a4;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall5(AMD64_SYSCALL_ID id, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5) {
    auto retval = static_cast<size_t>(id);
    register size_t r10 __asm__("r10") = a4;
    register size_t r8 __asm__("r8") = a5;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8) : "memory", "rcx", "r11");
    return retval;
}

size_t syscall6(AMD64_SYSCALL_ID id, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6) {
    auto retval = static_cast<size_t>(id);
    register size_t r8 __asm__("r8") = a5;
    register size_t r9 __asm__("r9") = a6;
    register size_t r10 __asm__("r10") = a4;
    __asm__ volatile("syscall" : "+a"(retval) : "D"(a1), "S"(a2), "d"(a3), "r"(r8), "r"(r9), "r"(r10) : "memory", "rcx", "r11");
    return retval;
}

void memcpy(void *dst, const void *src, size_t count) {
    auto *dst_ptr = static_cast<uint8_t *>(dst);
    const auto *src_ptr = static_cast<const uint8_t *>(src);

    while (count > 0) {
        *dst_ptr++ = *src_ptr++;
        count--;
    }
}

size_t write_stderr(const char *buf, size_t buf_len) {
    static_assert(sizeof(size_t) == sizeof(const char *));
    return syscall3(AMD64_SYSCALL_ID::WRITE, 2, reinterpret_cast<size_t>(buf), buf_len);
}

size_t puts(const char *str) { return write_stderr(str, strlen(str)); }

constexpr char utoa_lookup[] = "0123456789abcdefghijklmnopqrstuvwxyz";

void utoa(uint64_t v, char *buf, unsigned int base, unsigned int num_digits) {
    if ((base > strlen(utoa_lookup)) || (base < 2)) {
        panic("utoa: invalid base");
    }

    for (int j = num_digits - 1; j >= 0; j--) {
        buf[j] = utoa_lookup[v % base];
        v /= base;
    }
}

void print_hex8(uint8_t byte) {
    char str[2 + 2] = "0x";

    utoa(byte, &str[2], 16, 2);

    write_stderr(str, 2 + 2);
}

void print_hex16(uint16_t byte) {
    char str[2 + 4] = "0x";

    utoa(byte, &str[2], 16, 4);

    write_stderr(str, 2 + 4);
}

void print_hex32(uint32_t byte) {
    char str[2 + 8] = "0x";

    utoa(byte, &str[2], 16, 8);

    write_stderr(str, 2 + 8);
}

void print_hex64(uint64_t byte) {
    char str[2 + 16] = "0x";

    utoa(byte, &str[2], 16, 16);

    write_stderr(str, 2 + 16);
}

} // namespace helper
