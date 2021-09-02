#!/bin/bash

if [ "$#" -ne 1 ]
then
  echo "Usage: ${0} <path_to_riscv_gcc>"
  exit 1
fi


TXT_GREEN="\e[32m"
TXT_CLEAR="\e[0m"

cd "${0%/*}"
echo -e "${TXT_GREEN}Cleaning up leftovers...${TXT_CLEAR}"
set -x
rm -rf build_amd64
rm -rf build_rv64
rm -rf test_amd64
rm -rf test_trans
rm test_amd64.zip
rm test_trans.zip

# dont show the set +x command
{ set +x; } 2>/dev/null
set -e

echo -e "${TXT_GREEN}Building...${TXT_CLEAR}"
set -x
meson setup build_amd64 -Dcc=gcc
meson compile -C build_amd64

meson setup build_rv64 -Dcc=$1
meson compile -C build_rv64

{ set +x; } 2>/dev/null
echo -e "${TXT_GREEN}Translating...${TXT_CLEAR}"
set -x
cd build_rv64
../../../build/src/translate --output-binary=main.bin --output=translated.s main
gcc -c translated.s -o translated.o
ld -T ../../../src/generator/x86_64/helper/link.ld translated.o ../../../build/src/generator/x86_64/helper/libhelper.a -o translated

{ set +x; } 2>/dev/null
cd ..
echo -e "${TXT_GREEN}Testing if x86 and translated binaries produce same zip${TXT_CLEAR}"
set -x
./build_amd64/main test_amd64.zip main.c miniz.h zip.c zip.h
./build_rv64/translated test_trans.zip main.c miniz.h zip.c zip.h
cmp test_amd64.zip test_trans.zip

{ set +x; } 2>/dev/null
echo -e "${TXT_GREEN}Testing if x86 and translated binaries produce same content when extracting${TXT_CLEAR}"
set -x
./build_amd64/main -e test_amd64.zip test_amd64
./build_rv64/translated -e test_trans.zip test_trans

cmp test_amd64/main.c test_trans/main.c
cmp test_amd64/miniz.h test_trans/miniz.h
cmp test_amd64/zip.c test_trans/zip.c
cmp test_amd64/zip.h test_trans/zip.h

{ set +x; } 2>/dev/null
echo -e "${TXT_GREEN}Successfully tested the ZIP-Utility!${TXT_CLEAR}"
echo -e "${TXT_GREEN}Cleaning up...${TXT_CLEAR}"
set -x
rm -rf build_amd64
rm -rf build_rv64
rm -rf test_amd64
rm -rf test_trans
rm test_amd64.zip
rm test_trans.zip

{ set +x; } 2>/dev/null
exit 0
