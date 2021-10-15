# ERA Großpraktikum SoSe 2021: Statische Binärübersetzung von RISC-V in x86_64

## Build-Instruktionen

Dieses Projekt basiert auf [meson](https://mesonbuild.com).
Nach dem Klonen sind folgende Schritte auszuführen, um das Projekt zu bauen:

```sh
# Initiate and download submodules
git submodule update --init

# Project setup
meson setup build -Dbuildtype=release

# Build binaries:
ninja -C build all
```

Der Translator befindet sich in `build/src/translate`.
