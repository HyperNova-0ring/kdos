#include "../../hal.h"
#include "../../modules.h"
#include "vga.h"
#include "idt.h"
#include "multiboot2.h"

/* ── Console ─────────────────────────────────────────── */
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

/* ── Memory ──────────────────────────────────────────── */
// Memory map from Multiboot2 — filled in kernel.c
// and stored here for hal_mem_get_map to return
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

/* ── Arch init ───────────────────────────────────────── */
void hal_arch_init(uint64_t boot_magic, uint64_t boot_addr) {
    if (boot_magic != MULTIBOOT2_MAGIC)
        hal_panic("Multiboot2 magic is invalid");

    vga_print("Multiboot2 OK @ ");
    vga_print_hex(boot_addr);
    vga_print("\n");

    mb2_info_t *info = (mb2_info_t *)boot_addr;
    mb2_tag_t  *tag  = (mb2_tag_t *)((uint8_t *)info + 8);

    hal_mem_region_t regions[64];
    uint32_t         count = 0;

    while (tag->type != MB2_TAG_END) {
        if (tag->type == MB2_TAG_MMAP) {
            mb2_tag_mmap_t   *mmap  = (mb2_tag_mmap_t *)tag;
            mb2_mmap_entry_t *entry = (mb2_mmap_entry_t *)
                                      ((uint8_t *)mmap + sizeof(*mmap));
            mb2_mmap_entry_t *end   = (mb2_mmap_entry_t *)
                                      ((uint8_t *)mmap + mmap->size);
            while (entry < end && count < 64) {
                regions[count].base   = entry->base_addr;
                regions[count].length = entry->length;
                regions[count].type   = entry->type;
                count++;
                entry = (mb2_mmap_entry_t *)
                        ((uint8_t *)entry + mmap->entry_size);
            }
        }

        if (tag->type == MB2_TAG_MODULE) {
            mb2_tag_module_t *mod = (mb2_tag_module_t *)tag;
            modules_register((uintptr_t)mod->mod_start,
                             (uintptr_t)mod->mod_end,
                             mod->cmdline);
        }

        tag = MB2_TAG_NEXT(tag);
    }

    hal_mem_set_map(regions, count);
}

/* ── Interrupts ──────────────────────────────────────── */
void hal_idt_init(void) { idt_init(); }

/* ── Panic ───────────────────────────────────────────── */
void hal_panic(const char* msg) {
    hal_cpu_disable_interrupts();
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_print("\n\n  KERNEL PANIC: ");
    vga_print(msg);
    vga_print("  \n");
    hal_cpu_halt();
}
