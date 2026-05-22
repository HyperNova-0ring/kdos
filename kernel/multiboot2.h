#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include "hal.h"

#define MULTIBOOT2_MAGIC 0x36D76289

/* Tag type constants */
#define MB2_TAG_END          0
#define MB2_TAG_CMDLINE      1
#define MB2_TAG_MODULE       3
#define MB2_TAG_MMAP         6
#define MB2_TAG_FRAMEBUFFER  8

/* Multiboot2 info structure header */
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) mb2_info_t;

/* Generic tag header */
typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) mb2_tag_t;

/* Module tag (a module loaded by GRUB) */
typedef struct {
    uint32_t type;          // = 3
    uint32_t size;
    uint32_t mod_start;     /* physical start address — always 32-bit per MB2 spec */
    uint32_t mod_end;       /* physical end address — always 32-bit per MB2 spec */
    char     cmdline[];     /* module name and arguments */
} __attribute__((packed)) mb2_tag_module_t;

/* Memory map tag */
typedef struct {
    uint32_t type;          // = 6
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed)) mb2_tag_mmap_t;

/* Individual memory map entry */
typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;          // 1 = usable
    uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

/* Tag iteration: each tag is 8-byte aligned */
#define MB2_TAG_NEXT(tag) \
    ((mb2_tag_t*)(((uint8_t*)(tag)) + (((tag)->size + 7) & ~7)))

#endif