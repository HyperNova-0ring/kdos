#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include "hal.h"

#define MULTIBOOT2_MAGIC 0x36D76289

/* Tipos de tags en la estructura de info */
#define MB2_TAG_END          0
#define MB2_TAG_CMDLINE      1
#define MB2_TAG_MODULE       3
#define MB2_TAG_MMAP         6
#define MB2_TAG_FRAMEBUFFER  8

/* Cabecera de la estructura de info */
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) mb2_info_t;

/* Cabecera genérica de tag */
typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) mb2_tag_t;

/* Tag de módulo (un módulo cargado por GRUB) */
typedef struct {
    uint32_t type;          // = 3
    uint32_t size;
    uint32_t mod_start;     // dirección física de inicio
    uint32_t mod_end;       // dirección física de fin
    char     cmdline[];     // nombre/argumentos del módulo
} __attribute__((packed)) mb2_tag_module_t;

/* Tag de mapa de memoria */
typedef struct {
    uint32_t type;          // = 6
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed)) mb2_tag_mmap_t;

/* Entrada individual del mapa de memoria */
typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;          // 1 = usable
    uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

/* Iterar tags: cada tag está alineado a 8 bytes */
#define MB2_TAG_NEXT(tag) \
    ((mb2_tag_t*)(((uint8_t*)(tag)) + (((tag)->size + 7) & ~7)))

#endif