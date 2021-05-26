lifter_tests_src = ['test1.cpp']

test('lifter',
     executable('gtest-lifter',
                lifter_tests_src,
                dependencies : [
                    gtest_dep
                ],
                include_directories : inc,
                link_with : [lifter]))
