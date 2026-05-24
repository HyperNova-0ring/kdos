#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

/* ── Console ─────────────────────────────────────────── */
typedef enum {
    HAL_CONSOLE_VGA    = 0,
    HAL_CONSOLE_SERIAL = 1,
} hal_console_type_t;

hal_console_type_t hal_early_parse_console(uint64_t boot_addr);

void hal_console_init(hal_console_type_t type);
void hal_console_putchar(char c);
void hal_console_print(const char* str);
void hal_console_print_hex(uintptr_t value);
void hal_console_print_dec(uint64_t value);
void hal_console_clear(void);
int  hal_console_getchar(void);

/* ── Memory ──────────────────────────────────────────── */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} hal_mem_region_t;

#define PAGE_SIZE  4096UL

void     hal_mem_set_map(hal_mem_region_t* map, uint32_t count);
uint32_t hal_mem_get_map(hal_mem_region_t* out, uint32_t max);

void      hal_mem_init(void);
uintptr_t hal_mem_alloc_pages(uint32_t n);
void      hal_mem_free_pages(uintptr_t addr, uint32_t n);

/* ── CPU ─────────────────────────────────────────────── */
void hal_cpu_halt(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_enable_interrupts(void);

/* ── Arch init ───────────────────────────────────────── */
void hal_arch_init(uint64_t boot_magic, uint64_t boot_addr);

/* ── Interrupts ──────────────────────────────────────── */
void hal_idt_init(void);

/* Program-level exception hook (forwarded to IDT layer).
   fn receives (vector, rip, error_code, *out_rip, *out_rsp).
   Set *out_rip to redirect IRETQ; auto-cleared on entry. */
void hal_set_exc_hook(void (*fn)(uint64_t vec, uint64_t rip, uint64_t err,
                                  uint64_t* out_rip, uint64_t* out_rsp));
void hal_clear_exc_hook(void);

/* Sets red-on-white colors on VGA; no-op on serial.
   Call before printing an exception/panic message. */
void hal_console_panic_color(void);

/* ── Panic ───────────────────────────────────────────── */
void hal_panic(const char* msg);

#endif
