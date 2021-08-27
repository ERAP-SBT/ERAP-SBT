## Examples

riscv64_gcc = find_program('riscv64-linux-gnu-gcc', required: false)

if riscv64_gcc.found()
    custom_target('helloworld3',
        build_by_default: true,
        input: [ 'helloworld3.c' ],
        output: [ 'helloworld3' ],
        command: [ riscv64_gcc, '-static', '@INPUT@', '-o', '@OUTPUT@']
    )
endif
