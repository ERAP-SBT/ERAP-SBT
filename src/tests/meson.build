tests_src = [
    'test1.cpp'
]

test('translate',
     executable('gtest-all',
                 tests_src,
                 include_directories : inc,
                 dependencies : [ gtest_dep, gmock_dep ],
                 link_with : [ir, lifter, generator]
                ))
