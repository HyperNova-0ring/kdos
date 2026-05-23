#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

/* ── Console ─────────────────────────────────────────── */
typedef enum {
    HAL_CONSOLE_VGA    = 0,
    HAL_CONSOLE_SERIAL = 1,
} hal_console_type_t;

/* Parse the Multiboot2 boot info looking for console=vga|serial in the
   kernel cmdline. Returns HAL_CONSOLE_VGA if not found or boot_addr is 0. */
hal_console_type_t hal_early_parse_console(uint64_t boot_addr);

void hal_console_init(hal_console_type_t type);
void hal_console_putchar(char c);
void hal_console_print(const char* str);
void hal_console_print_hex(uintptr_t value);
void hal_console_print_dec(size_t value);
void hal_console_clear(void);
int  hal_console_getchar(void);   /* blocking: serial or PS/2 keyboard */

/* ── Memory ──────────────────────────────────────────── */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;          /* 1 = usable, 2 = reserved, etc. */
} hal_mem_region_t;

#define PAGE_SIZE  4096UL

/* Memory map (populated by hal_arch_init from the bootloader). */
void     hal_mem_set_map(hal_mem_region_t* map, uint32_t count);
uint32_t hal_mem_get_map(hal_mem_region_t* out, uint32_t max);

/* Physical page allocator — call hal_mem_init() before use. */
void      hal_mem_init(void);
uintptr_t hal_mem_alloc_pages(uint32_t n);      /* returns phys addr, 0 = OOM */
void      hal_mem_free_pages(uintptr_t addr, uint32_t n);

/* ── CPU ─────────────────────────────────────────────── */
void hal_cpu_halt(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_enable_interrupts(void);

/* ── Arch init ───────────────────────────────────────── */
/* Called once with the raw boot parameters (meaning is arch-specific).
   Responsible for validating the bootloader, populating the memory map
   and registering loaded modules. */
void hal_arch_init(uint64_t boot_magic, uint64_t boot_addr);

/* ── Interrupts ──────────────────────────────────────── */
void hal_idt_init(void);

/* ── Panic ───────────────────────────────────────────── */
void hal_panic(const char* msg);

#endif
