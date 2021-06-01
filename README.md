# ERA Großpraktikum SoSe 2021: Statische Binärübersetzung von RISC-V in x86_64

## Build-Instruktionen

Dieses Projekt basiert auf [meson](https://mesonbuild.com).

```sh
# Project setup
meson setup build -Db_coverage=true

# Build binaries:
ninja -C build all

# Run all tests:
ninja -C build test

# Generate coverage reports after running tests
ninja -C build coverage

# Build code documentation in 'build/docs' (requires doxygen)
ninja -C build docs
```

Before committing you can clean up your code by running `clang-format`:
```sh
ninja -C build clang-format
```
