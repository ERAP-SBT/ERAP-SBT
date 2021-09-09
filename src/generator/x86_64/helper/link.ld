ENTRY(_start);

SECTIONS {
    /*
     * riscv64-linux-gnu-gcc links starting with 0x10000 as default
     * we need a linker script, because -Wl,--section-start=.orig_binary=0x10000
     * will actually generate a program header LOAD starting at 0xf000, which linux
     * forbids
     */

    .orig_binary ABSOLUTE(orig_binary_vaddr) : {
        *(.orig_binary)

        /* account for bss memory etc. */
        . = ABSOLUTE(orig_binary_vaddr) + orig_binary_size;
    }

    /* place our things after the original binary */
    . = ABSOLUTE(orig_binary_vaddr) + orig_binary_size;
    .text : {
        *(.text)
    }
    .bss : {
        *(.bss)
    }
    .rodata : {
        *(.rodata)
    }
    .data : {
        *(.data)
    }
}
