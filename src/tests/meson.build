tests_src = [
    'test.cpp', 'argument_parser.cpp',

    # This is included here since argument_parser.cpp is part of main, which is an executable.
    '../argument_parser.cpp'
]

test('translate',
     executable('gtest-all',
                 tests_src,
                 include_directories : inc,
                 dependencies : [ gtest_dep, gmock_dep ],
                 link_with : [ir, lifter, generator]
                ))
