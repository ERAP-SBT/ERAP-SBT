# Compile with: riscv64-linux-gnu-gcc -static -nostdlib examples/helloworld.s -o examples/helloworld

.global _start

_start:
    /*
     * a0: int fd
     * a1: uint8_t *buf
     * a2: size_t buf_len
     * a7: RISCV64_EXIT
     */

    addi a0, x0, 1 # stdout
    la a1, helloworld
    addi a2, x0, 21
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

.section .rodata
helloworld:
.asciz "RISCV64 Hello World!\n"
