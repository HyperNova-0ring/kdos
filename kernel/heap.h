#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

void  heap_init(uintptr_t base);
void* heap_sbrk(intptr_t incr);

#endif
