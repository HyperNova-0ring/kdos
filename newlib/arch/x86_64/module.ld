/*
 * module.ld — linker script for CRT-based modules (x86_64).
 *
 * Layout at offset 0:
 *   [module_header_t]   — from .module_header section (MODULE_DEFINE macro)
 *   [_module_entry]     — from .text.entry (crt.o), immediately after header
 *   [module code/data]  — rest of the module
 *
 * The kernel computes entry = module_start + sizeof(module_header_t),
 * so .text.entry must follow .module_header with no padding.
 */

SECTIONS {
    . = 0;

    .module_header : {
        KEEP(*(.module_header))
    }

    .text ALIGN(1) : {
        *(.text.entry)          /* crt.o entry — must be first in .text */
        *(.text .text.*)
    }

    .rodata ALIGN(1) : { *(.rodata .rodata.*) }

    .data   ALIGN(1) : { *(.data .data.*) }

    .got    ALIGN(1) : { *(.got .got.*) }
    .got.plt ALIGN(1): { *(.got.plt) }

    .bss    ALIGN(1) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note.*)
    }
}
