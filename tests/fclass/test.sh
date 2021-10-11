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

rm -f fclass_test translated test_results.txt

{ set +x; } 2>/dev/null
set -e

echo -e "${TXT_BLUE}Building...${TXT_CLEAR}"

$1 -g -static -o fclass_test fclass_test.c -Wall -Wextra -Werror

{ set +x; } 2>/dev/null

echo -e "${TXT_BLUE}Translating...${TXT_CLEAR}"

set -x

../../build/src/translate --debug=false --output=translated fclass_test
../../build/src/translate --debug=false --output=opt_translated --optimize=all fclass_test

{ set +x; } 2>/dev/null


echo -e "${TXT_BLUE}Testing for the right result...${TXT_CLEAR}"

set -x

./translated > test_results.txt
./opt_translated > opt_test_results.txt
cmp correct_results.txt test_results.txt
cmp correct_results.txt opt_test_results.txt

{ set +x; } 2>/dev/null

echo -e "${TXT_BLUE}Successfully run the fclass test!${TXT_CLEAR}"
echo -e "${TXT_BLUE}Cleaning up...${TXT_CLEAR}"

set -x

rm fclass_test translated test_results.txt opt_translated opt_test_results.txt

{ set +x; } 2>/dev/null
exit 0
