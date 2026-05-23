#ifndef SERIAL_H
#define SERIAL_H

#ifndef __x86_64__
#error "serial.h is only for x86 implementation"
#endif

#include <stdint.h>

void serial_init(void);
void serial_putchar(char c);
void serial_print(const char* str);
void serial_print_hex(uint64_t value);

#endif
