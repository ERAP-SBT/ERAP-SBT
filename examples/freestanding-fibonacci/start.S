
.text
.global _start
_start:
    call main           // main method without inputs and return code in a0
    li a7, 93           // load terminate syscall code
    ecall

// signature: void print_str(char* str_addr, unsigned int len);
.global print_str
print_str:
    mv  a2, a1          // string length in a1
    mv  a1, a0          // string addr in a0
    li  a7, 64          // write syscall code linux
    li  a0, 1           // 1. syscall input: std_out
    ecall
    ret
