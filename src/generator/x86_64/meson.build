subdir('helper')

generator_sources = ['generator.cpp', 'reg_alloc_multi.cpp', 'helper/rv64_syscalls.cpp']
generator_x86_64 = static_library('generator_x86_64', generator_sources,
                           include_directories : inc)
