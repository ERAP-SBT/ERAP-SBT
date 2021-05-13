subdir('ir')
subdir('lifter')
subdir('generator')
subdir('tests')

executable('translate', 'main.cpp',
           include_directories : inc,
           link_with : [ir, lifter, generator])
