ENTRY(_start)

SECTIONS {
    . = 1M;

    /* PRIMERO el header multiboot — GRUB lo busca en los primeros 32KB */
    .multiboot ALIGN(8) : {
        KEEP(*(.multiboot))
    }

    /* DESPUÉS el código */
    .text ALIGN(4K) : {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata .rodata.*)
        *(.rodata.str1.8)
        *(.rodata.str1.1)
    }

    .data ALIGN(4K) : {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : {
        bss_start = .;      /* símbolo que usaremos en asm */
        *(COMMON)
        *(.bss .bss.*)
        bss_end = .;        /* símbolo de fin */
    }
}