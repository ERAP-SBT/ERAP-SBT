## Freestanding fibonacci

riscv64_gcc = find_program('riscv64-linux-gnu-gcc', required: false)

if riscv64_gcc.found()
    custom_target('helloworld',
        build_by_default: true,
        input: ['start.S', 'fibonacci.c'],
        output: ['fibonacci'],
        command: [ riscv64_gcc, '-g', '-static', '-ffreestanding', '-nostdlib', '@INPUT@', '-o', '@OUTPUT@', '-Werror', '-Wextra', '-Wall']
    )
endif
