project('simple_zip_utility', 'c', version : '0.1', default_options : [])

rv64_gcc_name = get_option('cc')
rv64_gcc = find_program(rv64_gcc_name, required: true)

custom_target('simple_zip_utility', 
    build_by_default: true, 
    input: [ 'main.c', 'zip.c' ], 
    output: ['main'],
    command: [rv64_gcc, '-static', '-g', '@INPUT@', '-o', '@OUTPUT@'])
