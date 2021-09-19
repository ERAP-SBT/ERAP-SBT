subdir('ir')
subdir('lifter')
subdir('generator')
subdir('tests')

executable('translate', ['main.cpp', 'argument_parser.cpp'],
           include_directories : inc,
           dependencies : [frvdec_dep, fadec_dep],
           link_with : [ir, lifter, generator],
           install: true)
