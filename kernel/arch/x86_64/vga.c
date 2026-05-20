#include "vga.h"

// Estado interno del terminal
static uint16_t* buffer;
static int       cursor_row;
static int       cursor_col;
static uint8_t   current_color;

// Combina foreground y background en 1 byte de color
static inline uint8_t make_color(vga_color fg, vga_color bg) {
    return (bg << 4) | fg;
}

// Combina ASCII + color en la entrada de 2 bytes del buffer
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

// Scroll: sube todas las filas una posición hacia arriba
static void scroll(void) {
    // Copiar fila N hacia fila N-1
    for (int row = 1; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            buffer[(row - 1) * VGA_COLS + col] =
                buffer[row * VGA_COLS + col];
        }
    }
    // Limpiar la última fila
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
        // Tab = avanzar hasta el próximo múltiplo de 4
        cursor_col = (cursor_col + 4) & ~3;
    } else {
        buffer[cursor_row * VGA_COLS + cursor_col] =
            make_entry(c, current_color);
        cursor_col++;
    }

    // Wrap horizontal
    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }

    // Scroll si llegamos al fondo
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