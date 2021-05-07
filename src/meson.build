subdir('ir')
subdir('lifter')
subdir('generator')

executable('translate', 'main.cpp',
           include_directories : inc,
           link_with : [ir, lifter, generator])
