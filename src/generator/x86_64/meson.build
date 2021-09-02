subdir('helper')

# Meson already requires python, so this doesn't add a new dependency
prog_python = import('python').find_installation('python3')

linker_script_cpp = custom_target(
  'linker_script.cpp',
  output: 'linker_script.cpp',
  input: ['helper/gen_ld_embed.py', 'helper/link.ld'],
  command: [prog_python, '@INPUT@', '@OUTPUT@'],
)

generator_sources = ['generator.cpp', linker_script_cpp]
generator_x86_64 = static_library('generator_x86_64', generator_sources,
                           include_directories : inc)
