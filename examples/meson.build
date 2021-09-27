## Examples

riscv64_gcc = find_program('riscv64-linux-gnu-gcc', required: false)

if riscv64_gcc.found()
    custom_target('helloworld2',
        build_by_default: true,
        input: ['helloworld2.s'],
        output: ['helloworld2'],
        command: [ riscv64_gcc, '-g', '-static', '-ffreestanding', '-nostdlib', '@INPUT@', '-o', '@OUTPUT@']
    )
    custom_target('helloworld3',
        build_by_default: true,
        input: [ 'helloworld3.c' ],
        output: [ 'helloworld3' ],
        command: [ riscv64_gcc, '-g', '-static', '@INPUT@', '-o', '@OUTPUT@']
    )

    signals_rv64 = custom_target('signals',
        input: [ 'signals.c' ],
        output: [ 'signals' ],
        command: [ riscv64_gcc, '-static', '@INPUT@', '-o', '@OUTPUT@']
    )

    custom_target('signals_naive',
        build_by_default: false,
        input: [ signals_rv64 ],
        output: [ 'signals_naive' ],
        command: [ translate, '--debug=false', '@INPUT@', '--output=@OUTPUT@' ]
    )

    custom_target('signals_interp_only',
        build_by_default: false,
        input: [ signals_rv64 ],
        output: [ 'signals_interp_only' ],
        command: [ translate, '--debug=false', '@INPUT@', '--output=@OUTPUT@', '--interpreter-only' ]
    )

    custom_target('signals_reg_alloc',
        build_by_default: false,
        input: [ signals_rv64 ],
        output: [ 'signals_reg_alloc' ],
        command: [ translate, '--debug=false', '@INPUT@', '--output=@OUTPUT@', '--disable-fp', '--optimize=generator' ]
    )

endif
