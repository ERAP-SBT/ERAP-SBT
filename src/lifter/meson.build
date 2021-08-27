lifter_sources = [
    'lifter.cpp',
    'elf_file.cpp',
    'program.cpp',
    'lift_utils.cpp',
    'split_basic_block.cpp',
    'backtracking.cpp',
    'instruction_parser.cpp',
    'instructions/arithmetic_logical.cpp',
    'instructions/cfc.cpp',
    'instructions/load_store.cpp',
    'instructions/mul_div.cpp',
    'instructions/shifts.cpp',
    'instructions/amo.cpp',
    'instructions/ziscr.cpp'
]

lifter = static_library('lifter', lifter_sources,
                        include_directories : inc,
                        dependencies : [frvdec_dep])

subdir('tests')
