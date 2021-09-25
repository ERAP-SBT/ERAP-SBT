.intel_syntax noprefix

.global unresolved_ijump
unresolved_ijump:
    call unresolved_ijump_handler

    /* the returned value is the address of the next compiled basic block */
    jmp rax


/* Implemented in assembly since sigreturn relies on stack position */

.global sh_signal_proxy_3
sh_signal_proxy_3:
    push rdi // signal
    push rsi // siginfo
    push rdx // ucontext

    lea rdi, [rsp + 24] // entry stack pointer
    call sh_enter_signal

    // Third argument (ucontext)
    pop rdx
    mov [register_file + (12*8)], rdx
    // Second argument (siginfo)
    pop rsi
    mov [register_file + (11*8)], rsi

    jmp signal_proxy_common

.global sh_signal_proxy_1
sh_signal_proxy_1:
    push rdi # signal

    lea rdi, [rsp + 8] // entry stack pointer
    call sh_enter_signal

    // fallthrough to signal_proxy_common

// signal on stack
signal_proxy_common:
    // First argument (signal)
    pop rdi
    movsx rdi, edi // signal is int
    mov [register_file + (10*8)], rdi

    // Return address
    mov rax, offset signal_trampoline_vaddr
    mov [register_file + (1*8)], rax

    // lookup signal (rdi) and get RV64 address
    call sh_lookup_signal_handler

    mov rbx, rax // save for later

    mov rdi, rax // RV64 address
    call ijump_lookup_for_addr

    // rax: x86 address or zero
    test rax, rax
    jnz 1f

    mov rdi, rbx // RV64 address
    call unresolved_ijump_handler

1:
    jmp rax


.global sh_signal_restorer
sh_signal_restorer:
    mov rdi, rsp
    call sh_exit_signal
    mov rsp, rax // Restore stack pointer if necessary

    mov rax, 15
    syscall // sigreturn
    ud2
