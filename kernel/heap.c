#include "heap.h"
#include "hal.h"

/*
 * Kernel heap — simple bump allocator backed by the PMM.
 *
 * At init time, HEAP_PAGES contiguous physical frames are reserved from
 * the PMM starting right after kernel_end. Because the kernel is
 * identity-mapped (phys == virt), those frames are directly addressable.
 *
 * sbrk(incr) advances the bump pointer within that reserved region and
 * returns the old top, matching the POSIX/newlib _sbrk contract.
 */

#define HEAP_PAGES  1024    /* 4 MB kernel heap */

static uintptr_t heap_base;
static uintptr_t heap_cur;
static uintptr_t heap_limit;

void heap_init(uintptr_t base) {
    heap_base = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uintptr_t phys = hal_mem_alloc_pages(HEAP_PAGES);
    if (phys == 0)
        hal_panic("heap_init: out of physical memory");
    if (phys != heap_base)
        hal_panic("heap_init: physical address mismatch — identity mapping broken?");

    heap_cur   = heap_base;
    heap_limit = heap_base + HEAP_PAGES * PAGE_SIZE;
}

void* heap_sbrk(intptr_t incr) {
    if (incr == 0) return (void*)heap_cur;
    if (incr < 0)  return (void*)-1;

    uintptr_t old = heap_cur;
    uintptr_t new = heap_cur + (uintptr_t)incr;

    if (new > heap_limit) return (void*)-1;  /* OOM */

    heap_cur = new;
    return (void*)old;
}
