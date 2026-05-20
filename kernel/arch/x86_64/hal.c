#include "hal.h"         // interfaz a implementar
#include "vga.h"         // implementación concreta x86_64

/* ── Consola ─────────────────────────────────────────── */
void hal_console_init(void)              { vga_init(); }
void hal_console_putchar(char c)         { vga_putchar(c); }
void hal_console_print(const char* str)  { vga_print(str); }
void hal_console_print_hex(uint64_t v)   { vga_print_hex(v); }
void hal_console_clear(void)             { vga_clear(); }

void hal_console_print_dec(uint64_t value) {
    if (value == 0) { vga_putchar('0'); return; }

    char buf[20];
    int  i = 19;
    buf[i] = '\0';

    while (value > 0) {
        buf[--i] = '0' + (value % 10);
        value   /= 10;
    }
    vga_print(&buf[i]);
}

/* ── Memoria ─────────────────────────────────────────── */
// El mapa de memoria viene de Multiboot2 — se llena en kernel.c
// y se guarda aquí para que hal_mem_get_map lo devuelva
static hal_mem_region_t mem_map[64];
static uint32_t         mem_map_count = 0;

void hal_mem_set_map(hal_mem_region_t* map, uint32_t count) {
    mem_map_count = count < 64 ? count : 64;
    for (uint32_t i = 0; i < mem_map_count; i++)
        mem_map[i] = map[i];
}

uint32_t hal_mem_get_map(hal_mem_region_t* out, uint32_t max) {
    uint32_t n = mem_map_count < max ? mem_map_count : max;
    for (uint32_t i = 0; i < n; i++)
        out[i] = mem_map[i];
    return n;
}

/* ── CPU ─────────────────────────────────────────────── */
void hal_cpu_halt(void) {
    while (1) __asm__ volatile ("hlt");
}

void hal_cpu_disable_interrupts(void) {
    __asm__ volatile ("cli");
}

void hal_cpu_enable_interrupts(void) {
    __asm__ volatile ("sti");
}

/* ── Panic ───────────────────────────────────────────── */
void hal_panic(const char* msg) {
    hal_cpu_disable_interrupts();
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_print("\n\n  KERNEL PANIC: ");
    vga_print(msg);
    vga_print("  \n");
    hal_cpu_halt();
}