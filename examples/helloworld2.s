.global _start

_start:
    la a3, helloworld
    call strlen

    /*
     * a0: int fd
     * a1: uint8_t *buf
     * a2: size_t buf_len
     * a7: RISCV64_WRITE
     */
    addi a0, x0, 1 # stdout
    la a1, helloworld
    addi a7, x0, 64
    ecall

    /*
     * a0: return code
     * a7: RISCV64_EXIT
     */
    addi a0, x0, 0
    addi a7, x0, 93
    ecall
_start_end:

.type _start,STT_FUNC
.size _start,_start_end-_start

/*
 * input: a3: buf
 * return: a2: string length
 */
strlen:
    addi a2, x0, 0
.Lloop:
    lbu a7, (a3)
    beq a7, x0, .Ldone
    addi a3, a3, 1
    addi a2, a2, 1
    j .Lloop
.Ldone:
    ret
strlen_end:

.type strlen,STT_FUNC
.size strlen,strlen_end-strlen

.section .rodata
helloworld:
.ascii "RISCV64 Hello World2 (strlen)!\n"
