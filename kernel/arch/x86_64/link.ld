ENTRY(_start)

SECTIONS {
    . = 1M;

    /* Multiboot header first — GRUB searches for it within the first 32KB */
    .multiboot ALIGN(8) : {
        KEEP(*(.multiboot))
    }

    /* Code section */
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
        bss_start = .;      /* symbol referenced in assembly */
        *(COMMON)
        *(.bss .bss.*)
        bss_end = .;
    }

    kernel_end = ALIGN(4K);
}