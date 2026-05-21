#ifndef MODULES_H
#define MODULES_H

#include <stdint.h>

#define MAX_MODULES 16

typedef struct {
    uintptr_t start;             // dirección física
    uintptr_t end;
    char     name[64];          // cmdline del módulo
    int      loaded;
} module_t;

typedef struct {
    module_t modules[MAX_MODULES];
    uint32_t count;
} module_list_t;

// Llenar la lista desde los tags Multiboot2
void     modules_init(void* mb2_info);
uint32_t modules_count(void);

// Buscar módulo por nombre (ej: "fat32.mod")
module_t* modules_find(const char* name);

// Devolver todos
module_list_t* modules_get_all(void);

#endif