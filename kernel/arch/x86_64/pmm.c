#include "pmm.h"
#include "../../hal.h"

/* Supports up to 512 MB of physical RAM (16 KB bitmap in BSS). */
#define MAX_FRAMES  (512UL * 1024 * 1024 / PAGE_SIZE)

/* Provided by the linker script — first byte after the kernel image. */
extern uintptr_t kernel_end;

static uint8_t  bitmap[MAX_FRAMES / 8];
static uint32_t total_frames;

static inline void frame_set_used(uint32_t f) { bitmap[f / 8] |=  (1u << (f % 8)); }
static inline void frame_set_free(uint32_t f) { bitmap[f / 8] &= ~(1u << (f % 8)); }
static inline int  frame_is_free (uint32_t f) { return !(bitmap[f / 8] & (1u << (f % 8))); }

void pmm_init(void) {
    hal_mem_region_t map[64];
    uint32_t count = hal_mem_get_map(map, 64);

    /* Start with everything marked used. */
    for (uint32_t i = 0; i < MAX_FRAMES / 8; i++)
        bitmap[i] = 0xFF;

    /* Determine total_frames from the highest usable address. */
    uintptr_t top = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (map[i].type == 1) {
            uintptr_t end = map[i].base + map[i].length;
            if (end > top) top = end;
        }
    }
    total_frames = (uint32_t)(top / PAGE_SIZE);
    if (total_frames > MAX_FRAMES) total_frames = MAX_FRAMES;

    /* Free every page inside a usable region (page-aligned bounds). */
    for (uint32_t i = 0; i < count; i++) {
        if (map[i].type != 1) continue;
        uintptr_t start = (map[i].base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uintptr_t end   = (map[i].base + map[i].length) & ~(PAGE_SIZE - 1);
        for (uintptr_t a = start; a < end; a += PAGE_SIZE) {
            uint32_t f = (uint32_t)(a / PAGE_SIZE);
            if (f < MAX_FRAMES) frame_set_free(f);
        }
    }

    /* Re-mark [0, kernel_end) as used — protects low memory and the kernel. */
    uintptr_t kend = ((uintptr_t)&kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    for (uintptr_t a = 0; a < kend; a += PAGE_SIZE) {
        uint32_t f = (uint32_t)(a / PAGE_SIZE);
        if (f < MAX_FRAMES) frame_set_used(f);
    }
}

uintptr_t pmm_alloc_pages(uint32_t n) {
    uint32_t run = 0, start = 0;

    for (uint32_t f = 0; f < total_frames; f++) {
        if (frame_is_free(f)) {
            if (run == 0) start = f;
            if (++run == n) {
                for (uint32_t i = start; i < start + n; i++)
                    frame_set_used(i);
                return (uintptr_t)start * PAGE_SIZE;
            }
        } else {
            run = 0;
        }
    }

    return 0;   /* OOM */
}

void pmm_free_pages(uintptr_t addr, uint32_t n) {
    uint32_t start = (uint32_t)(addr / PAGE_SIZE);
    for (uint32_t i = start; i < start + n; i++)
        frame_set_free(i);
}
