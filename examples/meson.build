## Examples

riscv64_gcc = find_program('riscv64-linux-gnu-gcc', required: false)

if riscv64_gcc.found()
    custom_target('helloworld2',
        build_by_default: true,
        input: ['helloworld2.s'],
        output: ['helloworld2'],
        command: [ riscv64_gcc, '-g', '-static', '-ffreestanding', '-nostdlib', '@INPUT@', '-o', '@OUTPUT@', '-Werror', '-Wextra', '-Wall']
    )
    custom_target('helloworld3',
        build_by_default: true,
        input: [ 'helloworld3.c' ],
        output: [ 'helloworld3' ],
        command: [ riscv64_gcc, '-g', '-static', '@INPUT@', '-o', '@OUTPUT@', '-Werror', '-Wextra', '-Wall']
    )
endif
