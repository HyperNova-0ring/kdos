#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

/* ── Consola ─────────────────────────────────────────── */
void hal_console_init(void);
void hal_console_putchar(char c);
void hal_console_print(const char* str);
void hal_console_print_hex(uint64_t value);
void hal_console_print_dec(uint64_t value);
void hal_console_clear(void);

/* ── Memoria ─────────────────────────────────────────── */
// Mapa de memoria físico entregado por el bootloader
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;          // 1 = usable, 2 = reserved, etc.
} hal_mem_region_t;

// Devuelve el número de regiones
void hal_mem_set_map(hal_mem_region_t* map, uint32_t count);
uint32_t hal_mem_get_map(hal_mem_region_t* out, uint32_t max);

/* ── CPU ─────────────────────────────────────────────── */
void hal_cpu_halt(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_enable_interrupts(void);

/* ── Panic ───────────────────────────────────────────── */
void hal_panic(const char* msg);

#endif
