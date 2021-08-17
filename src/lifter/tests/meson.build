lifter_tests_src = ['test_lifter.cpp','test_lift_arithmetical_logical.cpp', 'test_split_basic_block.cpp', 'test_float.cpp']

test('lifter',
     executable('gtest-lifter',
                lifter_tests_src,
                dependencies : [
                    gtest_dep,
                    frvdec_dep
                ],
                include_directories : inc,
                link_with : [lifter, ir]))
