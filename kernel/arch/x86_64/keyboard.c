#include "keyboard.h"
#include <stdint.h>

#define KBD_DATA   0x60
#define KBD_STATUS 0x64

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Scancode set 1 — unshifted */
static const char sc_normal[84] = {
    0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',  /* 0x00–0x07 */
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t', /* 0x08–0x0F */
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  /* 0x10–0x17 */
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',  /* 0x18–0x1F */
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  /* 0x20–0x27 */
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  /* 0x28–0x2F */
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  /* 0x30–0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,    /* 0x38–0x3F */
    0,    0,    0,    0,    0,    0,    0,    '7',  /* 0x40–0x47 */
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  /* 0x48–0x4F */
    '2',  '3',  '0',  '.',                           /* 0x50–0x53 */
};

/* Scancode set 1 — shifted */
static const char sc_shift[84] = {
    0,    0x1B, '!',  '@',  '#',  '$',  '%',  '^',  /* 0x00–0x07 */
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t', /* 0x08–0x0F */
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  /* 0x10–0x17 */
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',  /* 0x18–0x1F */
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  /* 0x20–0x27 */
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  /* 0x28–0x2F */
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  /* 0x30–0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,    /* 0x38–0x3F */
    0,    0,    0,    0,    0,    0,    0,    0,    /* 0x40–0x47 */
    0,    0,    '-',  0,    '5',  0,    '+',  0,    /* 0x48–0x4F */
    0,    0,    0,    0,                             /* 0x50–0x53 */
};

static int shift_held = 0;
static int extended   = 0;   /* 0xE0 prefix pending */

void keyboard_init(void) {
    /* Drain any stale bytes left in the controller output buffer */
    while (inb(KBD_STATUS) & 0x01)
        (void)inb(KBD_DATA);
}

/* Returns a non-zero ASCII character for each complete keypress, 0 for
   keys that produce no character (modifiers, arrows, function keys). */
static int kbd_process(uint8_t sc) {
    if (sc == 0xE0) {
        extended = 1;
        return 0;
    }

    if (extended) {
        extended = 0;
        /* Extended key release: just clear shift if needed (rare), ignore. */
        return 0;
    }

    int release = (sc & 0x80) != 0;
    uint8_t key = sc & 0x7F;

    /* Track shift state */
    if (key == 0x2A || key == 0x36) {   /* LShift / RShift */
        shift_held = release ? 0 : 1;
        return 0;
    }

    if (release || key >= 84)
        return 0;

    return (int)(uint8_t)(shift_held ? sc_shift[key] : sc_normal[key]);
}

int keyboard_getchar(void) {
    for (;;) {
        /* Wait until output buffer is full (bit 0 of status) */
        if (!(inb(KBD_STATUS) & 0x01))
            continue;

        uint8_t sc  = inb(KBD_DATA);
        int     ch  = kbd_process(sc);
        if (ch)
            return ch;
    }
}
