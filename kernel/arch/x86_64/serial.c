#include "serial.h"

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);    /* disable interrupts */
    outb(COM1 + 3, 0x80);    /* enable DLAB (baud divisor mode) */
    outb(COM1 + 0, 0x03);    /* divisor low byte  → 38400 baud */
    outb(COM1 + 1, 0x00);    /* divisor high byte */
    outb(COM1 + 3, 0x03);    /* 8N1, clear DLAB */
    outb(COM1 + 2, 0xC7);    /* enable FIFO, clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B);    /* RTS/DSR */
}

static int transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (!transmit_empty());
    outb(COM1, (uint8_t)c);
}

void serial_print(const char* str) {
    while (*str)
        serial_putchar(*str++);
}

void serial_print_hex(uint64_t value) {
    char buf[17];
    buf[16] = '\0';
    for (int i = 15; i >= 0; i--) {
        int digit = value & 0xF;
        buf[i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        value >>= 4;
    }
    serial_print("0x");
    serial_print(buf);
}
