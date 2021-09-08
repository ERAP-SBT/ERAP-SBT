helper_sources = ['helper.cpp']

runtime_dep_dir = get_option('datadir') / meson.project_name()

install_data('link.ld', install_dir: runtime_dep_dir)

if host_machine.system() == 'linux' and host_machine.cpu_family() == 'x86_64'
static_library('helper-x86_64',
    helper_sources,
    include_directories : inc,
    cpp_args: ['-ffreestanding', '-fno-exceptions', '-fno-stack-protector', '-nostdlib', '-fsanitize-undefined-trap-on-error'],
    override_options: ['b_sanitize=none', 'b_coverage=false'],
    install: true,
    install_dir: runtime_dep_dir
    )
endif
