ir_tests_src = ['test_ir_helpers.cpp', 'test_eval.cpp', 'test_const_folding.cpp']

test('ir',
     executable('gtest-ir',
                ir_tests_src,
                dependencies : [gtest_dep],
                include_directories : inc,
                link_with: [ir]))
