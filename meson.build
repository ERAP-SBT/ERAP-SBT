project('eragp-sbt-2021', 'cpp', version : '0.1', default_options : ['cpp_std=c++17', 'warning_level=3'])

gtest = subproject('gtest')
gtest_dep = gtest.get_variable('gtest_dep')
gmock_dep = gtest.get_variable('gmock_dep')

libfrvdec = subproject('frvdec')
frvdec_dep = libfrvdec.get_variable('frvdec')

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DDEBUG', language : 'cpp')
endif

inc = include_directories('include')

subdir('src')
