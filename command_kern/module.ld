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
    .got    ALIGN(1) : { *(.got .got.*)      }
    .got.plt ALIGN(1): { *(.got.plt)         }
    .bss    ALIGN(1) : {
        __bss_start = .;
        *(.bss .bss.*)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note.*)
    }
}
