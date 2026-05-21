SECTIONS {
    . = 0;

    .module_header : {
        KEEP(*(.module_header))
    }

    .text ALIGN(1) : {
        *(.text.entry)
        *(.text .text.*)
    }

    .rodata ALIGN(1) : { *(.rodata .rodata.*) }
    .data   ALIGN(1) : { *(.data .data.*)    }

    /* GOT necesaria para PIC */
    .got   ALIGN(1) : { *(.got .got.*) }
    .got.plt ALIGN(1) : { *(.got.plt) }

    .bss ALIGN(1) : { *(.bss .bss.*) }

    /* Descartar secciones innecesarias */
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
    }
}