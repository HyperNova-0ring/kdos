#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>

/* Load an ELF executable from a raw byte buffer into identity-mapped memory.
   Validates magic, class, machine type, and address bounds (all supplied by
   elf_arch.h from the target architecture directory).

   Each PT_LOAD segment is copied to its p_vaddr and its BSS tail is zeroed.
   Returns the entry-point virtual address, or 0 on any validation failure. */
uintptr_t elf_load(const void* data, size_t size);

#endif
