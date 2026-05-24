#include "elf.h"

/* elf_arch.h is provided per-architecture in arch/<arch>/elf_arch.h.
   The Makefile adds -I$(ARCH_DIR) so this resolves to the correct file. */
#include "elf_arch.h"

/* ── ELF64 structures ──────────────────────────────────────── */

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#define ET_EXEC   2
#define PT_LOAD   1
#define EI_CLASS  4   /* index into e_ident */

/* ── Internal helpers ──────────────────────────────────────── */

static void* km_memcpy(void* dst, const void* src, size_t n) {
    uint8_t*       d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dst;
}

static void km_memset(void* dst, int c, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) *d++ = (uint8_t)c;
}

/* ── Public loader ─────────────────────────────────────────── */

uintptr_t elf_load(const void* data, size_t size) {
    if (size < sizeof(elf64_ehdr_t)) return 0;

    const elf64_ehdr_t* eh = (const elf64_ehdr_t*)data;

    /* Magic */
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') return 0;

    /* Architecture validation — constants from elf_arch.h */
    if (eh->e_ident[EI_CLASS] != ELF_CLASS)  return 0;
    if (eh->e_type             != ET_EXEC)    return 0;
    if (eh->e_machine          != ELF_MACHINE) return 0;
    if (eh->e_phoff            == 0)          return 0;
    if (eh->e_phentsize        < sizeof(elf64_phdr_t)) return 0;

    /* Load PT_LOAD segments */
    int loaded = 0;

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        uint64_t off = eh->e_phoff + (uint64_t)i * eh->e_phentsize;
        if (off + sizeof(elf64_phdr_t) > size) return 0;

        const elf64_phdr_t* ph = (const elf64_phdr_t*)((const uint8_t*)data + off);
        if (ph->p_type != PT_LOAD) continue;

        /* Overflow-safe address and size checks */
        if (ph->p_vaddr < PROG_LOAD_MIN)                         return 0;
        if (ph->p_memsz  > PROG_LOAD_MAX - ph->p_vaddr)         return 0;
        if (ph->p_filesz > size || ph->p_offset > size - ph->p_filesz) return 0;

        /* Copy file image and zero BSS tail */
        km_memcpy((void*)ph->p_vaddr,
                  (const uint8_t*)data + ph->p_offset,
                  (size_t)ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            km_memset((void*)(ph->p_vaddr + ph->p_filesz), 0,
                      (size_t)(ph->p_memsz - ph->p_filesz));
        loaded = 1;
    }

    return loaded ? (uintptr_t)eh->e_entry : 0;
}
