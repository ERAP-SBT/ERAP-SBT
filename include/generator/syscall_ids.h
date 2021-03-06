#pragma once

#include <cstdint>

enum class RISCV_SYSCALL_ID : uint16_t {
    // should be passthrough but not sure yet
    /*IO_SETUP = 0,
    IO_DESTROY = 1,
    IO_SUBMIT = 2,
    IO_CANCEL = 3,
    IO_GET_EVENTS = 4,*/
    SETXATTR = 5,
    LSETXATTR = 6,
    FSETXATTR = 7,
    GETXATTR = 8,
    LGETXATTR = 9,
    FGETXATTR = 10,
    LISTXATTR = 11,
    LLISTXATTR = 12,
    FLISTXATTR = 13,
    REMOVEXATTR = 14,
    LREMOVEXATTR = 15,
    FREMOVEXATTR = 16,
    GETCWD = 17,
    LOOKUP_DCOOKIE = 18,
    EVENTFD2 = 19,
    EPOLL_CREATE1 = 20,
    EPOLL_CTL = 21,
    EPOLL_PWAIT = 22,
    DUP = 23,
    DUP3 = 24,
    FCNTL = 25,
    INOTIFY_INIT1 = 26,
    INOTIFY_ADD_WATCH = 27,
    INOTIFY_RM_WATCH = 28,
    IOCTL = 29,
    IOPRIO_SET = 30,
    IOPRIO_GET = 31,
    FLOCK = 32,
    MKNODAT = 33,
    MKDIRAT = 34,
    UNLINKAT = 35,
    SYMLINKAT = 36,
    LINKAT = 37,
    // is skipped
    UMOUNT2 = 39,
    MOUNT = 40,
    PIVOT_ROOT = 41,
    // nfs servectl, doesn't exist since linux 3.1
    STATFS = 43,
    FSTATFS = 44,
    TRUNCATE = 45,
    FTRUNCATE = 46,
    FALLOCATE = 47,
    FACCESSAT = 48,
    CHDIR = 49,
    FCHDIR = 50,
    CHROOT = 51,
    FCHMOD = 52,
    FCHMODAT = 53,
    FCHOWNAT = 54,
    FCHOWN = 55,
    OPENAT = 56,
    CLOSE = 57,
    VHANGUP = 58,
    PIPE2 = 59,
    QUOTACTL = 60,
    GETDENTS64 = 61,
    LSEEK = 62,
    READ = 63,
    WRITE = 64,
    READV = 65,
    WRITEV = 66,
    PREAD64 = 67,
    PWRITE64 = 68,
    PREADV = 69,
    PWRITEV = 70,
    SENDFILE = 71,
    PSELECT6 = 72,
    PPOLL = 73,
    SIGNALFD4 = 74,
    VMSPLICE = 75,
    SPLICE = 76,
    TEE = 77,
    READ_LINK_AT = 78,
    FSTATAT = 79,
    FSTAT = 80,
    SYNC = 81,
    FSYNC = 82,
    FDATASYNC = 83,
    SYNC_FILE_RANGE = 84,
    TIMERFD_CREATE = 85,
    TIMERFD_SETTIME = 86,
    TIMERFD_GETTIME = 87,
    UTIME_NS_AT = 88,
    ACCT = 89,
    CAPGET = 90,
    CAPSET = 91,
    PERSONALITY = 92,
    EXIT = 93,
    EXIT_GROUP = 94,
    WAITID = 95,
    SET_TID_ADDR = 96,
    UNSHARE = 97,
    FUTEX = 98,
    SET_ROBUST_LIST = 99,
    GET_ROBUST_LIST = 100,
    NANOSLEEP = 101,
    GETITIMER = 102,
    SETITIMER = 103,
    KEXEC_LOAD = 104,
    INIT_MODULE = 105,
    DELETE_MODULE = 106,
    TIMER_CREATE = 107,
    TIMER_GETTIME = 108,
    TIMER_GETOVERRUN = 109,
    TIMER_SETTIME = 110,
    TIMER_DELETE = 111,
    CLOCK_SETTIME = 112,
    CLOCK_GET_TIME = 113,
    CLOCK_GETRES = 114,
    CLOCK_NANOSLEEP = 115,
    SYSLOG = 116,
    PTRACE = 117,
    SCHED_SETPARAM = 118,
    SCHED_SETSCHEDULER = 119,
    SCHED_GETSCHEDULER = 120,
    SCHED_GETPARAM = 121,
    SCHED_SETAFFINITY = 122,
    SCHED_GETAFFINITY = 123,
    SCHED_YIELD = 124,
    SCHED_GET_PRIORITY_MAX = 125,
    SCHED_GET_PRIORITY_MIN = 126,
    SCHED_RR_GET_INTERVAL = 127,
    RESTART_SYSCALL = 128,
    KILL = 129,
    TKILL = 130,
    TGKILL = 131,
    SIGALTSTACK = 132,
    RT_SIGSUSPEND = 133,
    RT_SIGACTION = 134,
    RT_SIGPROCMASK = 135,
    RT_SIGPENDING = 136,
    RT_SIGTIMEDWAIT = 137,
    RT_SIGQUEUEINFO = 138,
    RT_SIGRETURN = 139,
    SETPRIORITY = 140,
    GETPRIORITY = 141,
    REBOOT = 142,
    SETREGID = 143,
    SETGID = 144,
    SETREUID = 145,
    SETUID = 146,
    SETRESUID = 147,
    GETRESUID = 148,
    SETRESGID = 149,
    GETRESGID = 150,
    SETFSUID = 151,
    SETFSGID = 152,
    TIMES = 153,
    SETPGID = 154,
    GETPGID = 155,
    GETSID = 156,
    SETSID = 157,
    GETGROUPS = 158,
    SETGROUPS = 159,
    UNAME = 160,
    SETHOSTNAME = 161,
    SETDOMAINNAME = 162,
    GETRLIMIT = 163,
    SETRLIMIT,
    GETRUSAGE,
    UMASK,
    PRCTL,
    GETCPU,
    GETTIMEOFDAY,
    SETTIMEOFDAY,
    ADJTIMEEX,
    GETPID,
    GETPPID,
    GETUID,
    GETEUID,
    GETGID,
    GETEGID,
    GETTID,
    SYSINFO,
    MQ_OPEN,
    MQ_UNLINK,
    MQ_TIMEDSEND,
    MQ_TIMEDRECEIVE,
    MQ_NOTIFY,
    MQ_GETSETATTR,
    MSGGET,
    MSGCTL,
    MSGRCV,
    MSGSND,
    SEMGET,
    SEMCTL,
    SEMTIMEDOP,
    SEMOP,
    SHMGET,
    SHMCTL,
    SHMAT,
    SHMDT,
    SOCKET,
    SOCKETPAIR,
    BIND,
    LISTEN,
    ACCEPT,
    CONNECT,
    GETSOCKETNAME,
    GETPEERNAME,
    SENDTO,
    RECVFROM,
    SETSOCKOPT,
    GETSOCKOPT,
    SHUTDOWN,
    SENDMSG,
    RECVMSG,
    READAHEAD,
    BRK = 214,
    MUNMAP = 215,
    MREMAP,
    ADD_KEY,
    REQUEST_KEY,
    KEYCTL,
    CLONE,
    EXECVE,
    MMAP = 222,
    FADVISE64,
    SWAPON,
    SWAPOFF,
    MPROTECT = 226,
    MSYNC,
    MLOCK,
    MUNLOCK,
    MLOCKALL,
    MUNLOCKALL,
    MINCORE,
    MADVISE = 233,
    REMAP_FILE_PAGES,
    MBIND,
    GET_MEMPOLICY,
    SET_MEMPOLICY,
    MIGRATE_PAGES,
    MOVE_PAGES,
    RT_TGSIGQUEUEINFO,
    PERF_EVENT_OPEN,
    ACCEPT4,
    RECVMMSG,
    FLUSH_ICACHE = 259, // _NR_arch_specific_syscall (=244) + 15
    WAIT4 = 260,
    PRLIMIT64,
    FANOTIFY_INIT,
    FANOTIFY_MARK,
    NAME_TO_HANDLE_AT,
    OPEN_BY_HANDLE_AT,
    CLOCK_ADJTIME,
    SYNCFS,
    SETNS,
    SENDMMSG,
    PROCESS_VM_READV,
    PROCESS_VM_WRITEV,
    KCMP,
    FINIT_MODULE,
    SCHED_SETATTR,
    SCHED_GETATTR,
    RENAMEAT2,
    SECCOMP,
    GETRANDOM,
    MEMFDCREATE,
    BPF,
    EXECVEAT,
    USERFAULTFD,
    MEMBARRIER,
    MLOCK2,
    COPY_FILE_RANGE,
    PREADV2,
    PWRITEV2,
    PKEY_MPROTECT,
    PKEY_ALLOC,
    PKEY_FREE,
    STATX,
    IO_PGETEVENTS,
    RSEQ,
    KEXEC_FILE_LOAD,
    PIDFD_SEND_SIGNAL = 424,
    IO_URING_SETUP,
    IO_URING_ENTER,
    IO_URING_REGISTER,
    OPEN_TREE,
    MOVE_MOUNT,
    FSOPEN,
    FSCONFIG,
    FSMOUNT,
    FSPICK,
    PIDFD_OPEN,
    CLONE3,
    CLOSE_RANGE,
    OPENAT2,
    PIDFD_GETFD,
    FACCESSAT2,
    PROCESS_MADVISE,
    EPOLL_PWAIT2,
    MOUNT_SETATTR,
    QUOTACTL_FD,
    LANDLOCK_CREATE_RULESET,
    LANDLOCK_ADD_RULE,
    LANDLOCK_RESTRICT_SELF,
    SYSCALL_ID_MAX = LANDLOCK_RESTRICT_SELF
};

enum class AMD64_SYSCALL_ID : uint16_t {
    READ = 0,
    WRITE = 1,
    CLOSE = 3,
    FSTAT = 5,
    LSEEK = 8,
    MMAP = 9,
    MPROTECT = 10,
    MUNMAP = 11,
    BRK = 12,
    RT_SIGACTION,
    RT_SIGPROCMASK,
    RT_SIGRETURN,
    IOCTL = 16,
    PREAD64 = 17,
    PWRITE64 = 18,
    READV = 19,
    WRITEV = 20,
    SCHED_YIELD = 24,
    MREMAP,
    MSYNC = 26,
    MINCORE,
    MADVISE = 28,
    SHMGET,
    SHMAT,
    SHMCTL,
    DUP = 32,
    NANOSLEEP = 35,
    GETITIMER,
    SETITIMER = 38,
    GETPID,
    SENDFILE = 40,
    SOCKET,
    CONNECT,
    ACCEPT,
    SENDTO,
    RECVFROM,
    SENDMSG,
    RECVMSG,
    SHUTDOWN = 48,
    BIND = 49,
    LISTEN,
    GETSOCKETNAME,
    GETPEERNAME,
    SOCKETPAIR = 53,
    SETSOCKOPT,
    GET_SOCKOPT,
    CLONE = 56,
    EXECVE = 59,
    EXIT = 60,
    WAIT4,
    KILL = 62,
    UNAME = 63,
    SEMGET,
    SEMOP,
    SEMCTL = 66,
    SHMDT,
    MSGGET = 68,
    MSGSND,
    MSGRCV,
    MSGCTL = 71,
    FCNTL = 72,
    FLOCK = 73,
    FSYNC,
    FDATASYNC,
    TRUNCATE = 76,
    FTRUNCATE = 77,
    GETCWD = 79,
    CHDIR = 80,
    FCHDIR = 81,
    FCHMOD = 91,
    FCHOWN = 93,
    UMASK = 95,
    GETTIMEOFDAY,
    GETRLIMIT = 97,
    GETRUSAGE,
    SYSINFO,
    TIMES = 100,
    PTRACE = 101,
    GETUID,
    SYSLOG = 103,
    GETGID,
    SETUID = 105,
    SETGID = 106,
    GETEUID,
    GETEGID,
    SETPGID = 109,
    GETPPID,
    SETSID = 112,
    SETREUID = 113,
    SETREGID = 114,
    GETGROUPS,
    SETGROUPS,
    SETRESUID = 117,
    GETRESUID,
    SETRESGID,
    GETRESGID,
    GETPGID,
    SETFSUID = 122,
    SETFSGID,
    GETSID,
    CAPGET = 125,
    CAPSET,
    RT_SIGPENDING,
    RT_SIGTIMEDWAIT,
    RT_SIGQUEUEINFO,
    RT_SIGSUSPEND = 130,
    SIGALTSTACK = 131,
    PERSONALITY = 135,
    STATFS = 137,
    FSTATFS = 138,
    GETPRIORITY = 140,
    SETPRIORITY,
    SCHED_SETPARAM = 142,
    SCHED_GETPARAM,
    SCHED_SETSCHEDULER,
    SCHED_GETSCHEDULER,
    SCHED_GET_PRIORITY_MAX,
    SCHED_GET_PRIORITY_MIN,
    SCHED_RR_GET_INTERVAL,
    MLOCK,
    MUNLOCK,
    MLOCKALL,
    MUNLOCKALL,
    VHANGUP = 153,
    PIVOT_ROOT = 155,
    PRCTL = 157,
    ADJTIMEX = 159,
    SETRLIMIT = 160,
    CHROOT = 161,
    SYNC = 162,
    ACCT,
    SETTIMEOFDAY,
    MOUNT = 165,
    UMOUNT2 = 166,
    SWAPON,
    SWAPOFF,
    REBOOT = 169,
    SETHOSTNAME,
    SETDOMAINNAME,
    INIT_MODULE = 175,
    DELETE_MODULE,
    QUOTACTL = 179,
    GETTID = 186,
    READAHEAD,
    SETXATTR = 188,
    LSETXATTR = 189,
    FSETXATTR = 190,
    GETXATTR = 191,
    LGETXATTR = 192,
    FGETXATTR = 193,
    LISTXATTR = 194,
    LLISTXATTR = 195,
    FLISTXATTR = 196,
    REMOVEXATTR = 197,
    LREMOVEXATTR = 198,
    FREMOVEXATTR = 199,
    TKILL,
    FUTEX = 202,
    SCHED_SETAFFINITY,
    SCHED_GETAFFINITY,
    LOOKUP_DCOOKIE = 212,
    REMAP_FILE_PAGES = 216,
    GETDENTS64 = 217,
    SET_TID_ADDR = 218,
    RESTART_SYSCALL,
    SEMTIMEDOP,
    FADVISE64,
    TIMER_CREATE = 222,
    TIMER_SETTIME,
    TIMER_GETTIME = 224,
    TIMER_GETOVERRUN,
    TIMER_DELETE,
    CLOCK_SETTIME,
    CLOCK_GET_TIME = 228,
    CLOCK_GETRES,
    CLOCK_NANOSLEEP,
    EXIT_GROUP = 231,
    EPOLL_CTL = 233,
    TGKILL = 234,
    MBIND = 237,
    SET_MEMPOLICY,
    GET_MEMPOLICY = 239,
    MQ_OPEN = 240,
    MQ_UNLINK,
    MQ_TIMEDSEND,
    MQ_TIMEDRECEIVE,
    MQ_NOTIFY,
    MQ_GETSETATTR,
    KEXEC_LOAD = 246,
    WAITID = 247,
    ADD_KEY,
    REQUEST_KEY,
    KEYCTL,
    IOPRIO_SET = 251,
    IOPRIO_GET = 252,
    INOTIFY_ADD_WATCH = 254,
    INOTIFY_RM_WATCH = 255,
    MIGRATE_PAGES,
    OPENAT = 257,
    MKDIRAT = 258,
    MKNODAT = 259,
    FCHOWNAT = 260,
    NEWFSTATAT = 262,
    UNLINKAT = 263,
    LINKAT = 265,
    SYMLINKAT = 266,
    READ_LINK_AT = 267,
    FCHMODAT = 268,
    FACCESSAT = 269,
    PSELECT6 = 270,
    PPOLL = 271,
    UNSHARE,
    SET_ROBUST_LIST,
    GET_ROBUST_LIST,
    SPLICE = 275,
    TEE = 276,
    SYNC_FILE_RANGE,
    VMSPLICE = 278,
    MOVE_PAGES,
    UTIME_NS_AT = 280,
    EPOLL_PWAIT = 281,
    TIMERFD_CREATE = 283,
    FALLOCATE = 285,
    TIMERFD_SETTIME,
    TIMERFD_GETTIME,
    ACCEPT4,
    SIGNALFD4 = 289,
    EVENTFD2 = 290,
    EPOLL_CREATE1 = 291,
    DUP3 = 292,
    PIPE2 = 293,
    INOTIFY_INIT1 = 294,
    PREADV = 295,
    PWRITEV,
    RT_TGSIGQUEUEINFO,
    PERF_EVENT_OPEN,
    RECVMMSG,
    FANOTIFY_INIT = 300,
    FANOTIFY_MARK,
    PRLIMIT64 = 302,
    NAME_TO_HANDLE_AT,
    OPEN_BY_HANDLE_AT,
    CLOCK_ADJTIME,
    SYNCFS,
    SENDMMSG,
    SETNS,
    GETCPU = 309,
    PROCESS_VM_READV,
    PROCESS_VM_WRITEV,
    KCMP,
    FINIT_MODULE,
    SCHED_SETATTR,
    SCHED_GETATTR,
    RENAMEAT2,
    SECCOMP,
    GETRANDOM,
    MEMFD_CREATE,
    KEXEC_FILE_LOAD,
    BPF = 321,
    EXECVEAT,
    USERFAULTFD,
    MEMBARRIER,
    MLOCK2,
    COPY_FILE_RANGE,
    PREADV2,
    PWRITEV2,
    PKEY_MPROTECT,
    PKEY_ALLOC,
    PKEY_FREE,
    STATX,
    IO_PGETEVENTS,
    RSEQ,
    PIDFD_SEND_SIGNAL = 424,
    IO_URING_SETUP,
    IO_URING_ENTER,
    IO_URING_REGISTER,
    OPEN_TREE,
    MOVE_MOUNT,
    FSOPEN,
    FSCONFIG,
    FSMOUNT,
    FSPICK,
    PIDFD_OPEN,
    CLONE3,
    CLOSE_RANGE,
    OPENAT2,
    PIDFD_GETFD,
    FACCESSAT2,
    PROCESS_MADVISE,
    EPOLL_PWAIT2,
    MOUNT_SETATTR,
    QUOTACTL_FD,
    LANDLOCK_CREATE_RULESET,
    LANDLOCK_ADD_RULE,
    LANDLOCK_RESTRICT_SELF,
    SYSCALL_ID_MAX = LANDLOCK_RESTRICT_SELF,
    SYSCALL_ID_INVALID
};
