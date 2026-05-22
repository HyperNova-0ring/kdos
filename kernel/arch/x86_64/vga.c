#include "vga.h"

// Internal terminal state
static uint16_t* buffer;
static int       cursor_row;
static int       cursor_col;
static uint8_t   current_color;

// Packs foreground and background into a 1-byte color value
static inline uint8_t make_color(vga_color fg, vga_color bg) {
    return (bg << 4) | fg;
}

// Packs an ASCII character and color into a 2-byte VGA entry
static inline uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_init(void) {
    buffer       = VGA_BUFFER;
    cursor_row   = 0;
    cursor_col   = 0;
    current_color = make_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

void vga_clear(void) {
    for (int row = 0; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            buffer[row * VGA_COLS + col] = make_entry(' ', current_color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
}

void vga_set_color(vga_color fg, vga_color bg) {
    current_color = make_color(fg, bg);
}

// Scroll: shift all rows up by one position
static void scroll(void) {
    // Copy row N into row N-1
    for (int row = 1; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            buffer[(row - 1) * VGA_COLS + col] =
                buffer[row * VGA_COLS + col];
        }
    }
    // Clear the last row
    for (int col = 0; col < VGA_COLS; col++) {
        buffer[(VGA_ROWS - 1) * VGA_COLS + col] =
            make_entry(' ', current_color);
    }
    cursor_row = VGA_ROWS - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        // Tab: advance to the next multiple of 4
        cursor_col = (cursor_col + 4) & ~3;
    } else {
        buffer[cursor_row * VGA_COLS + cursor_col] =
            make_entry(c, current_color);
        cursor_col++;
    }

    // Horizontal wrap
    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }

    // Scroll if we reach the bottom
    if (cursor_row >= VGA_ROWS) {
        scroll();
    }
}

void vga_print(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_print_hex(uint64_t value) {
    char buf[17];
    buf[16] = '\0';

    for (int i = 15; i >= 0; i--) {
        int digit = value & 0xF;
        buf[i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        value >>= 4;
    }

    vga_print("0x");
    vga_print(buf);
}