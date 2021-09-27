.intel_syntax noprefix

.global unresolved_ijump
unresolved_ijump:
    call unresolved_ijump_handler

    /* the returned value is the address of the next compiled basic block */
    jmp rax


/* Implemented in assembly since sigreturn relies on stack position */

.global sh_signal_proxy_1
sh_signal_proxy_1:
    // rdi: signal
    // set rsi and rdx to zero, then fall through
    xor rsi, rsi
    xor rdx, rdx

.global sh_signal_proxy_3
sh_signal_proxy_3:
    // rdi: signal, rsi: siginfo, rdx: ucontext
    mov rcx, rsp // entry stack pointer
    call sh_enter_signal
    // Handler address (x86) in rax

    jmp rax

.global sh_signal_restorer
sh_signal_restorer:
    mov rdi, rsp
    call sh_exit_signal
    mov rsp, rax // Restore stack pointer if necessary

    mov rax, 15
    syscall // sigreturn
    ud2
