#include "stddef.h"

#include <cstdint>
// TODO: define these structs ourselves?
#include "syscall_ids.h"

#include <linux/errno.h>
#include <sys/epoll.h>
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
} // namespace

extern "C" [[noreturn]] void panic(const char *err_msg);

// TODO: make a bitmap which syscalls are passthrough, which are not implemented
extern "C" uint64_t syscall_impl(uint64_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch (id) {
    case RISCV_SETXATTR:
        return syscall5(AMD64_SETXATTR, arg0, arg1, arg2, arg3, arg4);
    case RISCV_LSETXATTR:
        return syscall5(AMD64_LSETXATTR, arg0, arg1, arg2, arg3, arg4);
    case RISCV_FSETXATTR:
        return syscall5(AMD64_FSETXATTR, arg0, arg1, arg2, arg3, arg4);
    case RISCV_GETXATTR:
        return syscall4(AMD64_GETXATTR, arg0, arg1, arg2, arg3);
    case RISCV_LGETXATTR:
        return syscall4(AMD64_LGETXATTR, arg0, arg1, arg2, arg3);
    case RISCV_FGETXATTR:
        return syscall4(AMD64_FGETXATTR, arg0, arg1, arg2, arg3);
    case RISCV_LISTXATTR:
        return syscall3(AMD64_LISTXATTR, arg0, arg1, arg2);
    case RISCV_LLISTXATTR:
        return syscall3(AMD64_LLISTXATTR, arg0, arg1, arg2);
    case RISCV_FLISTXATTR:
        return syscall3(AMD64_FLISTXATTR, arg0, arg1, arg2);
    case RISCV_REMOVEXATTR:
        return syscall3(AMD64_REMOVEXATTR, arg0, arg1, arg2);
    case RISCV_LREMOVEXATTR:
        return syscall3(AMD64_LREMOVEXATTR, arg0, arg1, arg2);
    case RISCV_FREMOVEXATTR:
        return syscall3(AMD64_FREMOVEXATTR, arg0, arg1, arg2);
    case RISCV_GETCWD:
        return syscall2(AMD64_GETCWD, arg0, arg1);
    case RISCV_LOOKUP_DCOOKIE:
        return syscall3(AMD64_LOOKUP_DCOOKIE, arg0, arg1, arg2);
    case RISCV_EVENTFD2:
        return syscall2(AMD64_EVENTFD2, arg0, arg1);
    case RISCV_EPOLL_CREATE1:
        return syscall1(AMD64_EPOLL_CREATE1, arg0);
    case RISCV_EPOLL_CTL: {
        struct epoll_event event;
        auto *rv64_event = reinterpret_cast<rv64_epoll_event_t *>(arg3);
        if (rv64_event) {
            // can be null on CTL_DELETE
            event.data = rv64_event->data;
            event.events = rv64_event->events;
        }
        const auto res = syscall4(AMD64_EPOLL_CTL, arg0, arg1, arg2, reinterpret_cast<size_t>(&event));
        return res;
    }
    case RISCV_EPOLL_PWAIT: {
        struct epoll_event event;
        auto *rv64_event = reinterpret_cast<rv64_epoll_event_t *>(arg1);
        const auto res = syscall6(AMD64_EPOLL_PWAIT, arg0, reinterpret_cast<size_t>(&event), arg2, arg3, arg4, arg5);
        rv64_event->data = event.data;
        rv64_event->events = event.events;
        return res;
    }
    case RISCV_DUP:
        return syscall1(AMD64_DUP, arg0);
    case RISCV_DUP3:
        return syscall3(AMD64_DUP3, arg0, arg1, arg2);
    case RISCV_FCNTL:
        return syscall3(AMD64_FCNTL, arg0, arg1, arg2);
    case RISCV_INOTIFY_INIT1:
        return syscall1(AMD64_INOTIFY_INIT1, arg0);
    case RISCV_INOTIFY_ADD_WATCH:
        return syscall3(AMD64_INOTIFY_ADD_WATCH, arg0, arg1, arg2);
    case RISCV_INOTIFY_RM_WATCH:
        return syscall2(AMD64_INOTIFY_RM_WATCH, arg0, arg1);
    case RISCV_IOCTL:
        return syscall3(AMD64_IOCTL, arg0, arg1, arg2);
    case RISCV_IOPRIO_SET:
        return syscall3(AMD64_IOPRIO_SET, arg0, arg1, arg2);
    case RISCV_IOPRIO_GET:
        return syscall2(AMD64_IOPRIO_GET, arg0, arg1);
    case RISCV_FLOCK:
        return syscall2(AMD64_FLOCK, arg0, arg1);
    case RISCV_MKNODAT:
        return syscall4(AMD64_MKNODAT, arg0, arg1, arg2, arg3);
    case RISCV_MKDIRAT:
        return syscall3(AMD64_MKDIRAT, arg0, arg1, arg2);
    case RISCV_UNLINKAT:
        return syscall3(AMD64_UNLINKAT, arg0, arg1, arg2);
    case RISCV_SYMLINKAT:
        return syscall3(AMD64_SYMLINKAT, arg0, arg1, arg2);
    case RISCV_LINKAT:
        return syscall5(AMD64_LINKAT, arg0, arg1, arg2, arg3, arg4);
    case RISCV_UMOUNT2:
        return syscall2(AMD64_UMOUNT2, arg0, arg1);
    case RISCV_MOUNT:
        return syscall5(AMD64_MOUNT, arg0, arg1, arg2, arg3, arg4);
    case RISCV_PIVOT_ROOT:
        return syscall2(AMD64_PIVOT_ROOT, arg0, arg1);
    case RISCV_STATFS:
        return syscall2(AMD64_STATFS, arg0, arg1);
    case RISCV_FSTATFS:
        return syscall2(AMD64_FSTATFS, arg0, arg1);
    case RISCV_TRUNCATE:
        return syscall2(AMD64_TRUNCATE, arg0, arg1);
    case RISCV_FTRUNCATE:
        return syscall2(AMD64_FTRUNCATE, arg0, arg1);
    case RISCV_FALLOCATE:
        return syscall4(AMD64_FALLOCATE, arg0, arg1, arg2, arg3);
    case RISCV_FACCESSAT:
        return syscall3(AMD64_FACCESSAT, arg0, arg1, arg2);
    case RISCV_CHDIR:
        return syscall1(AMD64_CHDIR, arg0);
    case RISCV_FCHDIR:
        return syscall1(AMD64_FCHDIR, arg0);
    case RISCV_CHROOT:
        return syscall1(AMD64_CHROOT, arg0);
    case RISCV_FCHMOD:
        return syscall2(AMD64_FCHMOD, arg0, arg1);
    case RISCV_FCHMODAT:
        return syscall2(AMD64_FCHMODAT, arg0, arg1);
    case RISCV_FCHOWNAT:
        return syscall5(AMD64_FCHOWNAT, arg0, arg1, arg2, arg3, arg4);
    case RISCV_FCHOWN:
        return syscall3(AMD64_FCHOWN, arg0, arg1, arg2);
    case RISCV_OPENAT:
        return syscall3(AMD64_OPENAT, arg0, arg1, arg2);
    case RISCV_CLOSE:
        return syscall1(AMD64_CLOSE, arg0);
    case RISCV_VHANGUP:
        return syscall0(AMD64_VHANGUP);
    case RISCV_PIPE2:
        return syscall2(AMD64_PIPE2, arg0, arg1);
    case RISCV_QUOTACTL:
        return syscall4(AMD64_QUOTACTL, arg0, arg1, arg2, arg3);
    case RISCV_GETDENTS64:
        return syscall3(AMD64_GETDENTS64, arg0, arg1, arg2);
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
    case RISCV_PREAD64:
        return syscall4(AMD64_PREAD64, arg0, arg1, arg2, arg3);
    case RISCV_PWRITE64:
        return syscall4(AMD64_PWRITE64, arg0, arg1, arg2, arg3);
    case RISCV_PREADV:
        // TODO: the fourth arg is one 64bit arg but its split into high and low part on x64
        // check if its the same on riscv
        return syscall5(AMD64_PREADV, arg0, arg1, arg2, arg3, arg4);
    case RISCV_PWRITEV:
        // TODO: same as above
        return syscall5(AMD64_PWRITEV, arg0, arg1, arg2, arg3, arg4);
    case RISCV_SENDFILE:
        return syscall4(AMD64_SENDFILE, arg0, arg1, arg2, arg3);
    case RISCV_PSELECT6:
        return syscall6(AMD64_PSELECT6, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_PPOLL:
        return syscall5(AMD64_PPOLL, arg0, arg1, arg2, arg3, arg4);
    case RISCV_SIGNALFD4:
        return syscall4(AMD64_SIGNALFD4, arg0, arg1, arg2, arg3);
    case RISCV_VMSPLICE:
        return syscall4(AMD64_VMSPLICE, arg0, arg1, arg2, arg3);
    case RISCV_SPLICE:
        return syscall6(AMD64_SPLICE, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_TEE:
        return syscall4(AMD64_TEE, arg0, arg1, arg2, arg3);
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
    case RISCV_SYNC:
        return syscall0(AMD64_SYNC);
    case RISCV_FSYNC:
        return syscall1(AMD64_FSYNC, arg0);
    case RISCV_FDATASYNC:
        return syscall1(AMD64_FDATASYNC, arg0);
    case RISCV_SYNC_FILE_RANGE:
        return syscall4(AMD64_SYNC_FILE_RANGE, arg0, arg1, arg2, arg3);
    case RISCV_TIMERFD_CREATE:
        return syscall2(AMD64_TIMER_CREATE, arg0, arg1);
    case RISCV_TIMERFD_SETTIME:
        return syscall4(AMD64_TIMERFD_SETTIME, arg0, arg1, arg2, arg3);
    case RISCV_TIMERFD_GETTIME:
        return syscall2(AMD64_TIMERFD_GETTIME, arg0, arg1);
    case RISCV_UTIME_NS_AT:
        return syscall4(AMD64_UTIME_NS_AT, arg0, arg1, arg2, arg3);
    case RISCV_ACCT:
        return syscall1(AMD64_ACCT, arg0);
    case RISCV_CAPGET:
        return syscall2(AMD64_CAPGET, arg0, arg1);
    case RISCV_CAPSET:
        return syscall2(AMD64_CAPSET, arg0, arg1);
    case RISCV_PERSONALITY:
        return syscall1(AMD64_PERSONALITY, arg0);
    case RISCV_EXIT:
        return syscall1(AMD64_EXIT, arg0);
    case RISCV_EXIT_GROUP:
        return syscall1(AMD64_EXIT_GROUP, arg0);
    case RISCV_WAITID:
        // arg2 is siginfo* which i believe is the same
        // arg4 is a rusage* which is bigger on riscv but thats fine since the added bytes are reserved
        return syscall5(AMD64_WAITID, arg0, arg1, arg2, arg3, arg4);
    case RISCV_SET_TID_ADDR:
        return syscall1(AMD64_SET_TID_ADDR, arg0);
    case RISCV_UNSHARE:
        return syscall1(AMD64_UNSHARE, arg0);
    case RISCV_FUTEX:
        return syscall6(AMD64_FUTEX, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_SET_ROBUST_LIST:
        return syscall2(AMD64_SET_ROBUST_LIST, arg0, arg1);
    case RISCV_GET_ROBUST_LIST:
        return syscall3(AMD64_GET_ROBUST_LIST, arg0, arg1, arg2);
    case RISCV_NANOSLEEP:
        return syscall2(AMD64_NANOSLEEP, arg0, arg1);
    case RISCV_GETITIMER:
        return syscall2(AMD64_GETITIMER, arg0, arg1);
    case RISCV_SETITIMER:
        return syscall3(AMD64_SETITIMER, arg0, arg1, arg2);
    case RISCV_KEXEC_LOAD:
        [[fallthrough]];
    case RISCV_INIT_MODULE:
        [[fallthrough]];
    case RISCV_DELETE_MODULE:
        // better not
        return -ENOSYS;
    case RISCV_TIMER_CREATE:
        [[fallthrough]];
    case RISCV_TIMER_GETTIME:
        [[fallthrough]];
    case RISCV_TIMER_GETOVERRUN:
        [[fallthrough]];
    case RISCV_TIMER_SETTIME:
        [[fallthrough]];
    case RISCV_TIMER_DELETE:
        // needs signal support
        return -ENOSYS;
    case RISCV_CLOCK_SETTIME:
        return syscall2(AMD64_CLOCK_SETTIME, arg0, arg1);
    case RISCV_CLOCK_GET_TIME:
        return syscall2(AMD64_CLOCK_GET_TIME, arg0, arg1);
    case RISCV_CLOCK_GETRES:
        return syscall2(AMD64_CLOCK_GETRES, arg0, arg1);
    case RISCV_CLOCK_NANOSLEEP:
        return syscall4(AMD64_CLOCK_NANOSLEEP, arg0, arg1, arg2, arg3);
    case RISCV_SYSLOG:
        return syscall3(AMD64_SYSLOG, arg0, arg1, arg2);
    case RISCV_PTRACE:
        // deals with arch-specific data so better not
        return -ENOSYS;
    case RISCV_SCHED_SETPARAM:
        // riscv struct is bigger with reserved bytes again
        return syscall2(AMD64_SCHED_SETPARAM, arg0, arg1);
    case RISCV_SCHED_GETPARAM:
        return syscall2(AMD64_SCHED_GETPARAM, arg0, arg1);
    case RISCV_SCHED_SETSCHEDULER:
        return syscall3(AMD64_SCHED_SETSCHEDULER, arg0, arg1, arg2);
    case RISCV_SCHED_GETSCHEDULER:
        return syscall1(AMD64_SCHED_GETSCHEDULER, arg0);
    case RISCV_SCHED_SETAFFINITY:
        return syscall3(AMD64_SCHED_SETAFFINITY, arg0, arg1, arg2);
    case RISCV_SCHED_GETAFFINITY:
        return syscall3(AMD64_SCHED_GETAFFINITY, arg0, arg1, arg2);
    case RISCV_SCHED_YIELD:
        return syscall0(AMD64_SCHED_YIELD);
    case RISCV_SCHED_GET_PRIORITY_MAX:
        return syscall1(AMD64_SCHED_GET_PRIORITY_MAX, arg0);
    case RISCV_SCHED_GET_PRIORITY_MIN:
        return syscall1(AMD64_SCHED_GET_PRIORITY_MIN, arg0);
    case RISCV_SCHED_RR_GET_INTERVAL:
        return syscall2(AMD64_SCHED_RR_GET_INTERVAL, arg0, arg1);
    case RISCV_RESTART_SYSCALL:
        return syscall0(AMD64_RESTART_SYSCALL);
    case RISCV_KILL:
        return syscall2(AMD64_KILL, arg0, arg1);
    case RISCV_TKILL:
        return syscall2(AMD64_TKILL, arg0, arg1);
    case RISCV_TGKILL:
        return syscall3(AMD64_TGKILL, arg0, arg1, arg2);
    case RISCV_SIGALTSTACK:
        [[fallthrough]];
    case RISCV_RT_SIGACTION:
        [[fallthrough]];
    case RISCV_RT_SIGPROCMASK:
        // pretend they work
        return 0;
    case RISCV_RT_SIGSUSPEND:
        // we cant emulate that in a meaningful way so error out
        return -ENOSYS;
    case RISCV_RT_SIGPENDING:
        return syscall1(AMD64_RT_SIGPENDING, arg0);
    case RISCV_RT_SIGTIMEDWAIT:
        [[fallthrough]];
    case RISCV_RT_SIGQUEUEINFO:
        [[fallthrough]];
    case RISCV_RT_SIGRETURN:
        return -ENOSYS;
    case RISCV_SETPRIORITY:
        return syscall3(AMD64_SETPRIORITY, arg0, arg1, arg2);
    case RISCV_GETPRIORITY:
        return syscall2(AMD64_GETPRIORITY, arg0, arg1);
    case RISCV_REBOOT:
        return syscall4(AMD64_REBOOT, arg0, arg1, arg2, arg3);
    case RISCV_SETREGID:
        return syscall2(AMD64_SETREGID, arg0, arg1);
    case RISCV_SETGID:
        return syscall1(AMD64_SETGID, arg0);
    case RISCV_SETREUID:
        return syscall2(AMD64_SETREUID, arg0, arg1);
    case RISCV_SETUID:
        return syscall1(AMD64_SETUID, arg0);
    case RISCV_SETRESUID:
        return syscall3(AMD64_SETRESUID, arg0, arg1, arg2);
    case RISCV_GETRESUID:
        return syscall3(AMD64_GETRESUID, arg0, arg1, arg2);
    case RISCV_SETRESGID:
        return syscall3(AMD64_SETRESGID, arg0, arg1, arg2);
    case RISCV_GETRESGID:
        return syscall3(AMD64_GETRESGID, arg0, arg1, arg2);
    case RISCV_SETFSUID:
        return syscall1(AMD64_SETFSUID, arg0);
    case RISCV_SETFSGID:
        return syscall1(AMD64_SETFSGID, arg0);
    case RISCV_TIMES:
        return syscall1(AMD64_TIMES, arg0);
    case RISCV_SETPGID:
        return syscall2(AMD64_SETPGID, arg0, arg1);
    case RISCV_GETPGID:
        return syscall1(AMD64_GETPGID, arg0);
    case RISCV_GETSID:
        return syscall1(AMD64_GETSID, arg0);
    case RISCV_SETSID:
        return syscall0(AMD64_SETSID);
    case RISCV_GETGROUPS:
        return syscall2(AMD64_GETGROUPS, arg0, arg1);
    case RISCV_SETGROUPS:
        return syscall2(AMD64_SETGROUPS, arg0, arg1);
    case RISCV_UNAME:
        return syscall1(AMD64_UNAME, arg0);
    case RISCV_SETHOSTNAME:
        return syscall2(AMD64_SETHOSTNAME, arg0, arg1);
    case RISCV_SETDOMAINNAME:
        return syscall2(AMD64_SETDOMAINNAME, arg0, arg1);
    case RISCV_GETRLIMIT:
        return syscall2(AMD64_GETRLIMIT, arg0, arg1);
    case RISCV_SETRLIMIT:
        return syscall2(AMD64_SETRLIMI, arg0, arg1);
    case RISCV_GETRUSAGE:
        return syscall2(AMD64_GETRUSAGE, arg0, arg1);
    case RISCV_UMASK:
        return syscall1(AMD64_UMASK, arg0);
    case RISCV_PRCTL:
        return syscall5(AMD64_PRCTL, arg0, arg1, arg2, arg3, arg4);
    case RISCV_GETCPU:
        return syscall2(AMD64_GETCPU, arg0, arg1);
    case RISCV_GETTIMEOFDAY:
        return syscall2(AMD64_GETTIMEOFDAY, arg0, arg1);
    case RISCV_SETTIMEOFDAY:
        return syscall2(AMD64_SETTIMEOFDAY, arg0, arg1);
    case RISCV_ADJTIMEEX:
        return syscall1(AMD64_ADJTIMEX, arg0);
    case RISCV_GETPID:
        return syscall0(AMD64_GETPID);
    case RISCV_GETPPID:
        return syscall0(AMD64_GETPPID);
    case RISCV_GETUID:
        return syscall0(AMD64_GETUID);
    case RISCV_GETEUID:
        return syscall0(AMD64_GETEUID);
    case RISCV_GETGID:
        return syscall0(AMD64_GETGID);
    case RISCV_GETEGID:
        return syscall0(AMD64_GETEGID);
    case RISCV_GETTID:
        return syscall0(AMD64_GETTID);
    case RISCV_SYSINFO:
        return syscall1(AMD64_SYSINFO, arg0);
    case RISCV_MQ_OPEN:
        return syscall4(AMD64_MQ_OPEN, arg0, arg1, arg2, arg3);
    case RISCV_MQ_UNLINK:
        return syscall1(AMD64_MQ_UNLINK, arg0);
    case RISCV_MQ_TIMEDSEND:
        return syscall5(AMD64_MQ_TIMEDSEND, arg0, arg1, arg2, arg3, arg4);
    case RISCV_MQ_TIMEDRECEIVE:
        return syscall5(AMD64_MQ_TIMEDRECEIVE, arg0, arg1, arg2, arg3, arg4);
    case RISCV_MQ_NOTIFY:
        return syscall2(AMD64_MQ_NOTIFY, arg0, arg1);
    case RISCV_MQ_GETSETATTR:
        return syscall3(AMD64_MQ_GETSETATTR, arg0, arg1, arg2);
    case RISCV_MSGGET:
        return syscall2(AMD64_MSGGET, arg0, arg1);
    case RISCV_MSGCTL:
        return syscall3(AMD64_MSGCTL, arg0, arg1, arg2);
    case RISCV_MSGRCV:
        return syscall5(AMD64_MSGRCV, arg0, arg1, arg2, arg3, arg4);
    case RISCV_MSGSND:
        return syscall4(AMD64_MSGSND, arg0, arg1, arg2, arg3);
    case RISCV_SEMGET:
        return syscall3(AMD64_SEMGET, arg0, arg1, arg2);
    case RISCV_SEMCTL:
        return syscall4(AMD64_SEMCTL, arg0, arg1, arg2, arg3);
    case RISCV_SEMTIMEDOP:
        return syscall4(AMD64_SEMTIMEDOP, arg0, arg1, arg2, arg3);
    case RISCV_SEMOP:
        return syscall3(AMD64_SEMOP, arg0, arg1, arg2);
    case RISCV_SHMGET:
        return syscall3(AMD64_SHMGET, arg0, arg1, arg2);
    case RISCV_SHMCTL:
        return syscall3(AMD64_SHMCTL, arg0, arg1, arg2);
    case RISCV_SHMAT:
        return syscall3(AMD64_SHMAT, arg0, arg1, arg2);
    case RISCV_SHMDT:
        return syscall1(AMD64_SHMDT, arg0);
    case RISCV_SOCKET:
        [[fallthrough]];
    case RISCV_SOCKETPAIR:
        [[fallthrough]];
    case RISCV_BIND:
        [[fallthrough]];
    case RISCV_LISTEN:
        [[fallthrough]];
    case RISCV_ACCEPT:
        [[fallthrough]];
    case RISCV_CONNECT:
        [[fallthrough]];
    case RISCV_GETSOCKETNAME:
        [[fallthrough]];
    case RISCV_GETPEERNAME:
        [[fallthrough]];
    case RISCV_SENDTO:
        [[fallthrough]];
    case RISCV_RECVFROM:
        [[fallthrough]];
    case RISCV_SETSOCKOPT:
        [[fallthrough]];
    case RISCV_GETSOCKOPT:
        [[fallthrough]];
    case RISCV_SHUTDOWN:
        [[fallthrough]];
    case RISCV_SENDMSG:
        [[fallthrough]];
    case RISCV_RECVMSG:
        // need to check all the structs first
        return -ENOSYS;
    case RISCV_READAHEAD:
        return syscall3(AMD64_READAHEAD, arg0, arg1, arg2);
    case RISCV_BRK:
        return syscall1(AMD64_BRK, arg0);
    case RISCV_MUNMAP:
        return syscall2(AMD64_MUNMAP, arg0, arg1);
    case RISCV_MREMAP:
        return syscall5(AMD64_MREMAP, arg0, arg1, arg2, arg3, arg4);
    case RISCV_ADD_KEY:
        return syscall5(AMD64_ADD_KEY, arg0, arg1, arg2, arg3, arg4);
    case RISCV_REQUEST_KEY:
        return syscall4(AMD64_REQUEST_KEY, arg0, arg1, arg2, arg3);
    case RISCV_KEYCTL:
        return syscall5(AMD64_KEYCTL, arg0, arg1, arg2, arg3, arg4);
    case RISCV_MMAP:
        return syscall6(AMD64_MMAP, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_CLONE:
        [[fallthrough]];
    case RISCV_EXECVE:
        return -ENOSYS;
    case RISCV_FADVISE64:
        return syscall4(AMD64_FADVISE64, arg0, arg1, arg2, arg3);
    case RISCV_SWAPON:
        return syscall2(AMD64_SWAPON, arg0, arg1);
    case RISCV_SWAPOFF:
        return syscall1(AMD64_SWAPOFF, arg0);
    case RISCV_MPROTECT:
        return syscall3(AMD64_MPROTECT, arg0, arg1, arg2);
    case RISCV_MSYNC:
        return syscall3(AMD64_MSYNC, arg0, arg1, arg2);
    case RISCV_MLOCK:
        return syscall2(AMD64_MLOCK, arg0, arg1);
    case RISCV_MUNLOCK:
        return syscall2(AMD64_MUNLOCK, arg0, arg1);
    case RISCV_MLOCKALL:
        return syscall1(AMD64_MLOCKALL, arg0);
    case RISCV_MUNLOCKALL:
        return syscall0(AMD64_MUNLOCKALL);
    case RISCV_MINCORE:
        return syscall3(AMD64_MINCORE, arg0, arg1, arg2);
    case RISCV_MADVISE:
        return syscall3(AMD64_MADVISE, arg0, arg1, arg2);
    case RISCV_REMAP_FILE_PAGES:
        return syscall5(AMD64_REMAP_FILE_PAGES, arg0, arg1, arg2, arg3, arg4);
    case RISCV_MBIND:
        return syscall6(AMD64_MBIND, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_GET_MEMPOLICY:
        return syscall5(AMD64_GET_MEMPOLICY, arg0, arg1, arg2, arg3, arg4);
    case RISCV_SET_MEMPOLICY:
        return syscall3(AMD64_SET_MEMPOLICY, arg0, arg1, arg2);
    case RISCV_MIGRATE_PAGES:
        return syscall4(AMD64_MIGRATE_PAGES, arg0, arg1, arg2, arg3);
    case RISCV_MOVE_PAGES:
        return syscall6(AMD64_MOVE_PAGES, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_RT_TGSIGQUEUEINFO:
        [[fallthrough]];
    case RISCV_PERF_EVENT_OPEN:
        [[fallthrough]];
    case RISCV_ACCEPT4:
        [[fallthrough]];
    case RISCV_RECVMMSG:
        return -ENOSYS;
    case RISCV_FLUSH_ICACHE:
        return 0;
    case RISCV_WAIT4:
        return syscall4(AMD64_WAIT4, arg0, arg1, arg2, arg3);
    case RISCV_PRLIMIT64:
        return syscall4(AMD64_PRLIMIT64, arg0, arg1, arg2, arg3);
    case RISCV_FANOTIFY_INIT:
        return syscall2(AMD64_FANOTIFY_INIT, arg0, arg1);
    case RISCV_FANOTIFY_MARK:
        return syscall5(AMD64_FANOTIFY_MARK, arg0, arg1, arg2, arg3, arg4);
    case RISCV_NAME_TO_HANDLE_AT:
        return syscall5(AMD64_NAME_TO_HANDLE_AT, arg0, arg1, arg2, arg3, arg4);
    case RISCV_OPEN_BY_HANDLE_AT:
        return syscall3(AMD64_OPEN_BY_HANDLE_AT, arg0, arg1, arg2);
    case RISCV_CLOCK_ADJTIME:
        return syscall2(AMD64_CLOCK_ADJTIME, arg0, arg1);
    case RISCV_SYNCFS:
        return syscall1(AMD64_SYNCFS, arg0);
    case RISCV_SETNS:
        return syscall2(AMD64_SETNS, arg0, arg1);
    case RISCV_SENDMMSG:
        return -ENOSYS;
    case RISCV_PROCESS_VM_READV:
        return syscall6(AMD64_PROCESS_VM_READV, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_PROCESS_VM_WRITEV:
        return syscall6(AMD64_PROCESS_VM_WRITEV, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_KCMP:
        return syscall5(AMD64_KCMP, arg0, arg1, arg2, arg3, arg4);
    case RISCV_FINIT_MODULE:
        return -ENOSYS;
    case RISCV_SCHED_SETATTR:
        return syscall3(AMD64_SCHED_SETATTR, arg0, arg1, arg2);
    case RISCV_SCHED_GETATTR:
        return syscall4(AMD64_SCHED_GETATTR, arg0, arg1, arg2, arg3);
    case RISCV_RENAMEAT2:
        return syscall5(AMD64_RENAMEAT2, arg0, arg1, arg2, arg3, arg4);
    case RISCV_SECCOMP:
        // arch specific stuff?
        return syscall3(AMD64_SECCOMP, arg0, arg1, arg2);
    case RISCV_GETRANDOM:
        return syscall3(AMD64_GETRANDOM, arg0, arg1, arg2);
    case RISCV_MEMFDCREATE:
        return syscall2(AMD64_MEMFD_CREATE, arg0, arg1);
    case RISCV_BPF:
        return syscall3(AMD64_BPF, arg0, arg1, arg2);
    case RISCV_EXECVEAT:
        return -ENOSYS;
    case RISCV_USERFAULTFD:
        return syscall1(AMD64_USERFAULTFD, arg0);
    case RISCV_MEMBARRIER:
        return syscall3(AMD64_MEMBARRIER, arg0, arg1, arg2);
    case RISCV_MLOCK2:
        return syscall3(AMD64_MLOCK2, arg0, arg1, arg2);
    case RISCV_COPY_FILE_RANGE:
        return syscall6(AMD64_COPY_FILE_RANGE, arg0, arg1, arg2, arg3, arg4, arg5);
    case RISCV_PREADV2:
        return syscall5(AMD64_PREADV2, arg0, arg1, arg2, arg3, arg4);
    case RISCV_PWRITEV2:
        return syscall5(AMD64_PWRITEV2, arg0, arg1, arg2, arg3, arg4);
    case RISCV_PKEY_MPROTECT:
        return syscall4(AMD64_PKEY_MPROTECT, arg0, arg1, arg2, arg3);
    case RISCV_PKEY_ALLOC:
        return syscall2(AMD64_PKEY_ALLOC, arg0, arg1);
    case RISCV_PKEY_FREE:
        return syscall1(AMD64_PKEY_FREE, arg0);
    case RISCV_STATX:
        // the struct statx doesnt exist in our toolchain headers so im gonna assume they're equal
        return syscall5(AMD64_STATX, arg0, arg1, arg2, arg3, arg4);
    case RISCV_IO_PGETEVENTS:
        return -ENOSYS;
    case RISCV_RSEQ:
        return -ENOSYS;
    case RISCV_KEXEC_FILE_LOAD:
        return -ENOSYS;
    case RISCV_PIDFD_SEND_SIGNAL:
        return syscall4(AMD64_PIDFD_SEND_SIGNAL, arg0, arg1, arg2, arg3);
    case RISCV_IO_URING_SETUP:
        [[fallthrough]];
    case RISCV_IO_URING_ENTER:
        [[fallthrough]];
    case RISCV_IO_URING_REGISTER:
        // not sure about the structs
        return -ENOSYS;
    case RISCV_OPEN_TREE:
        return syscall3(AMD64_OPEN_TREE, arg0, arg1, arg2);
    case RISCV_MOVE_MOUNT:
        return syscall5(AMD64_MOVE_MOUNT, arg0, arg1, arg2, arg3, arg4);
    case RISCV_FSOPEN:
        return syscall2(AMD64_FSOPEN, arg0, arg1);
    case RISCV_FSCONFIG:
        return syscall5(AMD64_FSCONFIG, arg0, arg1, arg2, arg3, arg4);
    case RISCV_FSMOUNT:
        return syscall3(AMD64_FSMOUNT, arg0, arg1, arg2);
    case RISCV_FSPICK:
        return syscall3(AMD64_FSPICK, arg0, arg1, arg2);
    case RISCV_PIDFD_OPEN:
        return syscall2(AMD64_PIDFD_OPEN, arg0, arg1);
    case RISCV_CLONE3:
        return -ENOSYS;
    case RISCV_CLOSE_RANGE:
        return syscall3(AMD64_CLOSE_RANGE, arg0, arg1, arg2);
    case RISCV_OPENAT2:
        return syscall4(AMD64_OPENAT2, arg0, arg1, arg2, arg3);
    case RISCV_PIDFD_GETFD:
        return syscall3(AMD64_PIDFD_GETFD, arg0, arg1, arg2);
    case RISCV_FACCESSAT2:
        return syscall4(AMD64_FACCESSAT2, arg0, arg1, arg2, arg3);
    case RISCV_PROCESS_MADVISE:
        return syscall5(AMD64_PROCESS_MADVISE, arg0, arg1, arg2, arg3, arg4);
    case RISCV_EPOLL_PWAIT2: {
        struct epoll_event event;
        auto *rv64_event = reinterpret_cast<rv64_epoll_event_t *>(arg1);
        const auto res = syscall6(AMD64_EPOLL_PWAIT2, arg0, reinterpret_cast<size_t>(&event), arg2, arg3, arg4, arg5);
        rv64_event->data = event.data;
        rv64_event->events = event.events;
        return res;
    }
    case RISCV_MOUNT_SETATTR:
        return syscall5(AMD64_MOUNT_SETATTR, arg0, arg1, arg2, arg3, arg4);
    case RISCV_QUOTACTL_FD:
        return syscall4(AMD64_QUOTACTL_FD, arg0, arg1, arg2, arg3);
    case RISCV_LANDLOCK_CREATE_RULESET:
        return syscall3(AMD64_LANDLOCK_CREATE_RULESET, arg0, arg1, arg2);
    case RISCV_LANDLOCK_ADD_RULE:
        return syscall4(AMD64_LANDLOCK_ADD_RULE, arg0, arg1, arg2, arg3);
    case RISCV_LANDLOCK_RESTRICT_SELF:
        return syscall2(AMD64_LANDLOCK_RESTRICT_SELF, arg0, arg1);
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
