#include "transfer.h"

/*
 * COM2 (0x2F8) — used exclusively for ELF file transfer.
 * Initialized lazily on first call to transfer_recv_elf.
 * Uses 115200 baud (divisor=1), 8N1, FIFO enabled.
 */

#define COM2 0x2F8

#define KELF_MAX_SIZE (2u * 1024u * 1024u)   /* 2 MB hard cap */

static int com2_initialized = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void com2_init(void) {
    outb(COM2 + 1, 0x00);   /* disable interrupts         */
    outb(COM2 + 3, 0x80);   /* DLAB on                    */
    outb(COM2 + 0, 0x01);   /* divisor low  = 1           */
    outb(COM2 + 1, 0x00);   /* divisor high = 0  → 115200 */
    outb(COM2 + 3, 0x03);   /* 8N1, DLAB off              */
    outb(COM2 + 2, 0xC7);   /* FIFO on, clear, thr=14     */
    outb(COM2 + 4, 0x0B);   /* RTS/DSR                    */
    com2_initialized = 1;
}

static uint8_t com2_recv_byte(void) {
    while (!(inb(COM2 + 5) & 0x01));
    return inb(COM2);
}

static void com2_recv_buf(uint8_t* buf, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        buf[i] = com2_recv_byte();
}

int transfer_recv_elf(const kst_t* kst, uint8_t** out_buf, uint32_t* out_size) {
    if (!com2_initialized) com2_init();

    /* Wait for KELF magic */
    uint8_t magic[4];
    com2_recv_buf(magic, 4);
    if (magic[0] != 'K' || magic[1] != 'E' ||
        magic[2] != 'L' || magic[3] != 'F')
        return 0;

    /* Read size (little-endian uint32_t) */
    uint8_t szb[4];
    com2_recv_buf(szb, 4);
    uint32_t size = (uint32_t)szb[0]
                  | ((uint32_t)szb[1] <<  8)
                  | ((uint32_t)szb[2] << 16)
                  | ((uint32_t)szb[3] << 24);

    if (size == 0 || size > KELF_MAX_SIZE) return 0;

    /* Allocate via kernel heap */
    void* buf = kst->mem.sbrk((intptr_t)size);
    if (buf == (void*)-1) return 0;

    /* Receive ELF data */
    com2_recv_buf((uint8_t*)buf, size);

    *out_buf  = (uint8_t*)buf;
    *out_size = size;
    return 1;
}
