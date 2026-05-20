#ifndef VGA_H
#define VGA_H

// Este archivo es exclusivo de x86_64
#ifndef __x86_64__
#error "vga.h is only for x86 bios impl."
#endif

#include <stdint.h>
#include <stddef.h>

#define VGA_BUFFER ((uint16_t*)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

typedef enum {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
} vga_color;

void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_print(const char* str);
void vga_print_hex(uint64_t value);
void vga_set_color(vga_color fg, vga_color bg);

#endif