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
rm -f test_amd64.zip
rm -f test_trans.zip

# dont show the set +x command
{ set +x; } 2>/dev/null
set -e

echo -e "${TXT_GREEN}Building...${TXT_CLEAR}"
set -x
mkdir build_amd64
gcc -g -static -o build_amd64/main main.c zip.c -Wall -Wextra

mkdir build_rv64
$1 -g -static -o build_rv64/main main.c zip.c -Wall -Wextra

{ set +x; } 2>/dev/null
echo -e "${TXT_GREEN}Translating...${TXT_CLEAR}"
set -x
cd build_rv64
../../../build/src/translate --debug=false --output=translated main --disable-fp
../../../build/src/translate --debug=false --output=optimized main --disable-fp --optimize=all
../../../build/src/translate --debug=false --output=interpreter main --disable-fp --interpreter-only

{ set +x; } 2>/dev/null
cd ..
echo -e "${TXT_GREEN}Testing if x86 and translated binaries produce same zip${TXT_CLEAR}"
set -x
./build_amd64/main test_amd64.zip main.c miniz.h zip.c zip.h
./build_rv64/translated test_trans.zip main.c miniz.h zip.c zip.h
./build_rv64/interpreter test_interpreter.zip main.c miniz.h zip.c zip.h
./build_rv64/optimized test_opt.zip main.c miniz.h zip.c zip.h

cmp test_amd64.zip test_trans.zip
cmp test_amd64.zip test_interpreter.zip
cmp test_amd64.zip test_opt.zip

{ set +x; } 2>/dev/null
echo -e "${TXT_GREEN}Testing if x86 and translated binaries produce same content when extracting${TXT_CLEAR}"
set -x
./build_amd64/main -e test_amd64.zip test_amd64
./build_rv64/translated -e test_trans.zip test_trans
./build_rv64/interpreter -e test_interpreter.zip test_interpreter
./build_rv64/optimized -e test_opt.zip test_opt

cmp test_amd64/main.c test_trans/main.c
cmp test_amd64/miniz.h test_trans/miniz.h
cmp test_amd64/zip.c test_trans/zip.c
cmp test_amd64/zip.h test_trans/zip.h

cmp test_amd64/main.c test_interpreter/main.c
cmp test_amd64/miniz.h test_interpreter/miniz.h
cmp test_amd64/zip.c test_interpreter/zip.c
cmp test_amd64/zip.h test_interpreter/zip.h

cmp test_amd64/main.c test_opt/main.c
cmp test_amd64/miniz.h test_opt/miniz.h
cmp test_amd64/zip.c test_opt/zip.c
cmp test_amd64/zip.h test_opt/zip.h

{ set +x; } 2>/dev/null
echo -e "${TXT_GREEN}Successfully tested the ZIP-Utility!${TXT_CLEAR}"
echo -e "${TXT_GREEN}Cleaning up...${TXT_CLEAR}"
set -x
rm -rf build_amd64
rm -rf build_rv64
rm -rf test_amd64
rm -rf test_trans
rm -rf test_interpreter
rm -rf test_opt
rm test_amd64.zip
rm test_trans.zip
rm test_interpreter.zip
rm test_opt.zip

{ set +x; } 2>/dev/null
exit 0
