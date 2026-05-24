#include "../../hal.h"
#include "../../modules.h"
#include "vga.h"
#include "serial.h"
#include "keyboard.h"
#include "idt.h"
#include "multiboot2.h"
#include "pmm.h"

static hal_console_type_t active_console = HAL_CONSOLE_VGA;

/* ── Early cmdline parse ─────────────────────────────── */

static int cmdline_has(const char* haystack, const char* needle) {
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*n && *h == *n) { h++; n++; }
        if (!*n) return 1;
    }
    return 0;
}

hal_console_type_t hal_early_parse_console(uint64_t boot_addr) {
    if (!boot_addr) return HAL_CONSOLE_VGA;

    mb2_info_t *info = (mb2_info_t *)boot_addr;
    mb2_tag_t  *tag  = (mb2_tag_t *)((uint8_t *)info + 8);

    while (tag->type != MB2_TAG_END) {
        if (tag->type == MB2_TAG_CMDLINE) {
            mb2_tag_cmdline_t *cl = (mb2_tag_cmdline_t *)tag;
            if (cmdline_has(cl->string, "console=serial"))
                return HAL_CONSOLE_SERIAL;
            return HAL_CONSOLE_VGA;
        }
        tag = MB2_TAG_NEXT(tag);
    }
    return HAL_CONSOLE_VGA;
}

/* ── Console ─────────────────────────────────────────── */

void hal_console_init(hal_console_type_t type) {
    active_console = type;
    if (type == HAL_CONSOLE_SERIAL) {
        serial_init();
    } else {
        vga_init();
        keyboard_init();
    }
}

void hal_console_putchar(char c) {
    if (active_console == HAL_CONSOLE_SERIAL)
        serial_putchar(c);
    else
        vga_putchar(c);
}

void hal_console_print(const char* str) {
    if (active_console == HAL_CONSOLE_SERIAL)
        serial_print(str);
    else
        vga_print(str);
}

void hal_console_print_hex(uint64_t v) {
    if (active_console == HAL_CONSOLE_SERIAL)
        serial_print_hex(v);
    else
        vga_print_hex(v);
}

void hal_console_clear(void) {
    if (active_console == HAL_CONSOLE_VGA)
        vga_clear();
    /* serial has no clear */
}

int hal_console_getchar(void) {
    if (active_console == HAL_CONSOLE_SERIAL)
        return serial_getchar();
    return keyboard_getchar();
}

void hal_console_print_dec(uint64_t value) {
    if (value == 0) { hal_console_putchar('0'); return; }

    char buf[20];
    int  i = 19;
    buf[i] = '\0';

    while (value > 0) {
        buf[--i] = '0' + (value % 10);
        value   /= 10;
    }
    hal_console_print(&buf[i]);
}

/* ── Memory ──────────────────────────────────────────── */

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

    hal_console_print("Multiboot2 OK @ ");
    hal_console_print_hex(boot_addr);
    hal_console_print("\n");

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

/* ── Physical memory allocator ───────────────────────── */

void      hal_mem_init(void)                             { pmm_init(); }
uintptr_t hal_mem_alloc_pages(uint32_t n)                { return pmm_alloc_pages(n); }
void      hal_mem_free_pages(uintptr_t addr, uint32_t n) { pmm_free_pages(addr, n); }

/* ── Interrupts ──────────────────────────────────────── */

void hal_idt_init(void) { idt_init(); }

void hal_set_exc_hook(void (*fn)(uint64_t vec, uint64_t rip, uint64_t err,
                                  uint64_t* out_rip, uint64_t* out_rsp)) {
    idt_set_exc_hook(fn);
}
void hal_clear_exc_hook(void) { idt_clear_exc_hook(); }

/* ── Console color ───────────────────────────────────── */

void hal_console_panic_color(void) {
    if (active_console == HAL_CONSOLE_VGA)
        vga_set_color(VGA_WHITE, VGA_RED);
}

/* ── Panic ───────────────────────────────────────────── */

void hal_panic(const char* msg) {
    hal_cpu_disable_interrupts();
    hal_console_panic_color();
    hal_console_print("\n\n  KERNEL PANIC: ");
    hal_console_print(msg);
    hal_console_print("  \n");
    hal_cpu_halt();
}
