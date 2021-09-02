# This script generates a cpp file with an array containing the bytes of the linker script.
# argv[1] is the linker script, and argv[2] is the output file.

import sys

with open(sys.argv[1], "rb") as ld_file:
    linker_script = ld_file.read()

with open(sys.argv[2], "w") as header_file:
    header_file.write("char LINKER_SCRIPT[] = {")
    for b in linker_script:
        header_file.write(f"{hex(b)}, ")
    header_file.write("0x00};\n")
