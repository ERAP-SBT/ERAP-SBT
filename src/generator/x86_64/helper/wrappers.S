.global unresolved_ijump
unresolved_ijump:
    call unresolved_ijump_handler

    /* the returned value is the address of the next compiled basic block */
    jmp %rax
