subdir('x86_64')

generator_sources = ['generator.cpp']
generator = static_library('generator', generator_sources,
                           include_directories : inc,
                           link_with : [generator_x86_64])

subdir('tests')
