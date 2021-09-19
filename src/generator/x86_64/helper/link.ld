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

    /* use the precalculated addresses for the sections */
    .tbss ABSOLUTE(tbss_start) : {
        *(.tbss)
    }

    .ijumps ABSOLUTE(ijump_table_start) : {
        *(.ijumps)
    }

    .ttext ABSOLUTE(ttext_start) : {
        *(.ttext)
    }

    .rodata ABSOLUTE(rodata_start) : {
        *(.rodata)
    }

    /* place helper things after the precalculated stuff */
    . = ABSOLUTE(helper_start);
    .test_sec : {
        *(.text)
    }
    .bss : {
        *(.bss)
    }
    .data : {
        *(.data)
    }
    .eh_frame : {
        *(.eh_frame)
    }
}
