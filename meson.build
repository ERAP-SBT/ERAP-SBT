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

# Docs target (this must be run manually)
doxygen = find_program('doxygen', required : false)
if doxygen.found()
  message('Doxygen was found, documentation can be built using the "docs" target')

  cdata = configuration_data()

  cdata.set('VERSION', meson.project_version())
  cdata.set('ROOTDIR', meson.source_root())

  if find_program('dot', required : false).found()
    cdata.set('HAVE_DOT', 'YES')
  else
    cdata.set('HAVE_DOT', 'NO')
  endif

  doxyfile = configure_file(
      input : 'Doxyfile.in',
      output : 'Doxyfile',
      configuration : cdata,
      install : false)
  custom_target('docs',
      input : doxyfile,
      output : 'docs',
      command : [doxygen, doxyfile])
endif
