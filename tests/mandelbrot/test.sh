#!/bin/bash

if [ "$#" -ne 1 ]
then
  echo "Usage: ${0} <path_to_riscv_gcc>"
  exit 1
fi

TXT_BLUE="\e[36m"
TXT_CLEAR="\e[0m"

cd "${0%/*}"

echo -e "${TXT_BLUE}Cleaning up leftovers...${TXT_CLEAR}"

set -x

rm -rf build_amd64 build_rv64
rm amd64_mandelbrot.txt rv64_mandelbrot.txt interpreter_mandelbrot.txt

{ set +x; } 2>/dev/null
set -e

echo -e "${TXT_BLUE}Building...${TXT_CLEAR}"
mkdir build_amd64
mkdir build_rv64

set -x

gcc -g -static -o build_amd64/mandelbrot mandelbrot.c  -Wall -Wextra -O3
# -march=native: generate fma3 instructions if supported
gcc -march=native -g -static -o build_amd64/mandelbrot_fma mandelbrot.c  -Wall -Wextra -O3
$1 -g -static -o build_rv64/mandelbrot mandelbrot.c -Wall -Wextra -O3

{ set +x; } 2>/dev/null

echo -e "${TXT_BLUE}Translating...${TXT_CLEAR}"
cd build_rv64

set -x

../../../build/src/translate --debug=false --output=translated mandelbrot
../../../build/src/translate --debug=false --output=interpreter mandelbrot --interpreter-only

{ set +x; } 2>/dev/null

cd ..
echo -e "${TXT_BLUE}Testing for the right result...${TXT_CLEAR}"

set -x

build_amd64/mandelbrot > amd64_mandelbrot.txt
build_amd64/mandelbrot_fma > amd64_mandelbrot_fma.txt
build_rv64/translated > rv64_mandelbrot.txt
build_rv64/interpreter > interpreter_mandelbrot.txt

cmp amd64_mandelbrot.txt rv64_mandelbrot.txt
cmp amd64_mandelbrot_fma.txt interpreter_mandelbrot.txt

{ set +x; } 2>/dev/null

echo -e "${TXT_BLUE}Successfully run the mandelbrot test!${TXT_CLEAR}"
echo -e "${TXT_BLUE}Cleaning up...${TXT_CLEAR}"

set -x

rm -rf build_amd64 build_rv64
rm amd64_mandelbrot.txt amd64_mandelbrot_fma.txt rv64_mandelbrot.txt interpreter_mandelbrot.txt

{ set +x; } 2>/dev/null
exit 0
