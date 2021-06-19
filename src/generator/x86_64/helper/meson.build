helper_sources = ['helper.cpp']

if host_machine.system() == 'linux' and host_machine.cpu_family() == 'x86_64'
static_library('helper', helper_sources, cpp_args: ['-ffreestanding', '-fno-exceptions', '-nostdlib'])
endif