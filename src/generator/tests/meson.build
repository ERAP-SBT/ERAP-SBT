tests_src = [
    'sanity_test.cpp', 'test_irs.cpp'
]

test('generator',
     executable('gtest-generator',
                 tests_src,
                 include_directories : inc,
                 dependencies : [ gtest_dep, gmock_dep, fadec_dep ],
                 link_with : [ir, generator]
                ))
