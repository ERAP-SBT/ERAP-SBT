# ERA Großpraktikum SoSe 2021: Statische Binärübersetzung von RISC-V in x86_64

## Build instructions

This project ist based on [meson](https://mesonbuild.com).

To build this project, execute `setup_and_build.sh` or execute following steps:

```bash
# Initiate and download submodules
git submodule update --init

# Project setup (change for the usage of sanitizers in debug mode,
# for more information look at the meson documentation)
meson setup build -Dbuildtype=release

# Builds the project in the build folder with the parameters set above.
ninja -C build all
```

The translator can now be found in `build/src/translate`.
If your want to move the translator executable, your need to specify the path
to the helper library and the linkerscript manually. To do so, you use
the command line options `--helper-path` and `--linkerscript-path` when starting the translation.
Of course, these runtime components also can be moved somewhere else on the system.
The default paths are:

- for the helper library: `build/src/generator/x86_64/helper/libhelper-x86_64.a`
- for the linkerscript: `src/generator/x86_64/helper/link.ld`

# Usage

For translating the binary file "examples/hellworld" which is build by the default if the RISC-V GCC is found on the system.

The command to run the simple translation can look like this, when executed in the build folder:

```bash
src/translate examples/helloworld2 --output=translated_helloworld2
```

In order to activate all optimizations:

```bash
src/translate examples/helloworld2 --output=translated_helloworld2 optimize=all
```

Optimizations can also specified further. To activate all lifter optimizations and the usage of register allocation:

```bash
src/translate examples/helloworld2 --output=translated_helloworld2 optimize=lifter,reg_alloc
```

With the specification of the default paths for helper library and linkerscript:

```bash
src/translate examples/helloworld2 --output=translated_helloworld2 --helper-path=src/generator/x86_64/helper/libhelper-x86_64.a --linkerscript-path=../src/generator/x86_64/helper/link.ld
```

To only use the interpreter, a slow RISC-V emulator which is part of the helper library:

```bash
# starts the translation process
src/translate examples/helloworld2 --output=translated_helloworld2 --interpreter-only

# runs the interpreter
./translated_helloworld2
```

All options can be listed using the command line option `--help`, especially all implemented optimizations.

# Restrictions

Currently supported binaries must fulfill following criteria:

- RISC-V ELF binaries
- Only RV32IMAFDC and/or RV64IMAFDC, no other extensions are currently supported and their instructions will be ignored
- Statically linked
- Little endian format
- System V ABI

The floating point implementation can and will be imprecise because of the differences between the floating point implementation
in x86_64 and RISC-V. Especially when dealing with unsigned integer conversions and NaNs some errors might occur.
Such errors could be avoided using a soft-float implementation, but this would decrease the performance massively.
Therefore feel free to report and fix such issues or develop a fast soft-float implementation.

Another restriction is that programs with multiple threads will not work despite the support for the A extension.
That is because the instructions of the A extensions are implemented as if they weren't atomic to be able to translate
programs compiled linked with the glibc, but not to be forced to design an concept for concurrency.

If you are planning to translate huge binaries (> 100MB), be prepared for a massive consumption of RAM (at least 32GiB!).
