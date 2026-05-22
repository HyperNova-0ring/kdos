#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE  4096UL

void      pmm_init(void);
uintptr_t pmm_alloc_pages(uint32_t n);
void      pmm_free_pages(uintptr_t addr, uint32_t n);

#endif
