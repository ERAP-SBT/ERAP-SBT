helper_sources = ['helper.cpp']

if host_machine.system() == 'linux'
static_library('helper', helper_sources, cpp_args: ['-ffreestanding', '-fno-exceptions', '-nostdlib'])
endif