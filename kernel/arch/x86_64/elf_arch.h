#ifndef ELF_ARCH_H
#define ELF_ARCH_H

/* Architecture-specific ELF constants for x86_64.
   Included by kernel/elf.c via -I$(ARCH_DIR).
   Each supported arch must provide its own elf_arch.h. */

#define ELF_CLASS    2     /* ELFCLASS64                  */
#define ELF_MACHINE  62    /* EM_X86_64                   */

/* Identity-mapped window in which programs may be loaded.
   The HAL establishes this range in boot.S (0–128 MB huge pages).
   Programs must sit above the kernel (1 MB) and inside the map. */
#define PROG_LOAD_MIN  0x100000UL    /* 1 MB  — above BIOS/kernel  */
#define PROG_LOAD_MAX  0x8000000UL   /* 128 MB — identity-map limit */

#endif
