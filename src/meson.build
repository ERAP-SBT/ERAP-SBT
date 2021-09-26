runtime_dep_dir = get_option('datadir') / meson.project_name()

subdir('ir')
subdir('lifter')
subdir('generator')
subdir('tests')

conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set('runtime_dep_dir', runtime_dep_dir)
conf_data.set('binary_dir', get_option('bindir'))

configure_file(input : 'config.h.in',
    output : 'config.h',
    configuration : conf_data)
configuration_inc = include_directories('.')

executable('translate', ['main.cpp', 'argument_parser.cpp'],
           include_directories : [inc, configuration_inc],
           dependencies : [frvdec_dep],
           link_with : [ir, lifter, generator],
           install: true)
