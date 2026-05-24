/*
 * prog.ld — linker script for ELF programs running on KDOS (x86_64).
 *
 * Programs are loaded at runtime by command_kern's ELF loader.
 * The loader reads p_vaddr from each PT_LOAD segment and copies it
 * directly into the identity-mapped physical address space (0-128 MB).
 *
 * Default load address: 0x2000000 (32 MB) — safely above the kernel,
 * its heap, and any GRUB-loaded modules.  Override with -Ttext=ADDR
 * if you need a different base.
 *
 * Entry ABI: _module_entry(const kst_t* kst)
 *   kst arrives in %rdi per the SysV x86_64 ABI.
 *   crt.S zeroes BSS, stores kst in _kst, then calls main(0, NULL).
 */

ENTRY(_prog_entry)

SECTIONS {
    . = 0x2000000;

    .text : {
        *(.text.entry)          /* crt.o _prog_entry — must be first */
        *(.text .text.*)
    }

    .rodata ALIGN(8) : { *(.rodata .rodata.*) }

    .data   ALIGN(8) : { *(.data .data.*) }

    .got    ALIGN(8) : { *(.got .got.*) }
    .got.plt ALIGN(8): { *(.got.plt) }

    .bss    ALIGN(8) : {
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
