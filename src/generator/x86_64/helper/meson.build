helper_sources = ['helper.cpp', 'interpreter.cpp', 'rv64_syscalls.cpp', 'wrappers.S']

runtime_dep_dir = get_option('datadir') / meson.project_name()

install_data('link.ld', install_dir: runtime_dep_dir)

if host_machine.system() == 'linux' and host_machine.cpu_family() == 'x86_64'
    static_library('helper-x86_64',
        helper_sources,
        include_directories : inc,
        cpp_args: ['-ffreestanding', '-fno-exceptions', '-fno-stack-protector', '-nostdlib', '-fsanitize-undefined-trap-on-error'],

        # Even if the translator is build with sanitzing / coverage we can't have them in the helper
        # library, as it targets a standalone environment without stdlibc
        override_options: ['b_sanitize=none', 'b_coverage=false'],
        install: true,
        install_dir: runtime_dep_dir,
        dependencies: [frvdec_dep],
        )
endif
