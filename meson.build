project('eragp-sbt-2021', 'cpp',
        default_options : ['cpp_std=c++17', 'warning_level=3'])

libfrvdec = subproject('frvdec')
frvdec_dep = libfrvdec.get_variable('frvdec')

inc = include_directories('include')

subdir('src')
