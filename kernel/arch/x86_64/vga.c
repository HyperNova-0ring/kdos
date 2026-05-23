#include "vga.h"

#define VGA_CRTC_ADDR 0x3D4
#define VGA_CRTC_DATA 0x3D5

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint16_t* buffer;
static int       cursor_row;
static int       cursor_col;
static uint8_t   current_color;

static void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_row * VGA_COLS + cursor_col);
    outb(VGA_CRTC_ADDR, 0x0F);
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(VGA_CRTC_ADDR, 0x0E);
    outb(VGA_CRTC_DATA, (uint8_t)(pos >> 8));
}

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
    vga_update_cursor();
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
        cursor_col = (cursor_col + 4) & ~3;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_COLS - 1;
        }
        /* Move only — the caller's ' ' will overwrite the cell */
    } else {
        buffer[cursor_row * VGA_COLS + cursor_col] =
            make_entry(c, current_color);
        cursor_col++;
    }

    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }

    if (cursor_row >= VGA_ROWS) {
        scroll();
    }

    vga_update_cursor();
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