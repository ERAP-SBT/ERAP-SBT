helper_sources = ['helper.cpp']

if host_machine.system() == 'linux' and host_machine.cpu_family() == 'x86_64'
    static_library('helper',
        helper_sources,
        cpp_args: ['-ffreestanding', '-fno-exceptions', '-nostdlib', '-fsanitize-undefined-trap-on-error'],
        pic: true,
        dependencies: [frvdec_dep],

        # Even if the translator is build with sanitzing / coverage we can't have them in the helper
        # library, as it targets a standalone environment without stdlibc
        override_options: ['b_sanitize=none', 'b_coverage=false'],
    )
endif
