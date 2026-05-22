#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

/* ── Console ─────────────────────────────────────────── */
void hal_console_init(void);
void hal_console_putchar(char c);
void hal_console_print(const char* str);
void hal_console_print_hex(uintptr_t value);
void hal_console_print_dec(size_t value);
void hal_console_clear(void);

/* ── Memory ──────────────────────────────────────────── */
// Physical memory map provided by the bootloader
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;          // 1 = usable, 2 = reserved, etc.
} hal_mem_region_t;

// Returns the number of regions
void hal_mem_set_map(hal_mem_region_t* map, uint32_t count);
uint32_t hal_mem_get_map(hal_mem_region_t* out, uint32_t max);

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
