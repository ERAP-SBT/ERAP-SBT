#! /bin/bash

# This bash script automatically setups and builds the project

# Initiate and download submodules
git submodule update --init

# Project setup (change for the usage of sanitizers in debug mode, for more information look at the meson documentation)
meson setup build -Dbuildtype=release

# Builds the project in the build folder with the parameters set above.
# The binary now is in 'build/src/translate'. If moving the binary, you'd need to
# manually set the command line options '--helper-path' and '--linkerscript-path'.
# The helper library can be found in 'build/src/generator/x86_64/helper/libhelper-x86_64.a'.
# The linkerscript can be found in 'src/generator/x86_64/helper/link.ld'.
ninja -C build all
