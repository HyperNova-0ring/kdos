#ifndef MODULE_ABI_H
#define MODULE_ABI_H

#include "hal.h"

#define MODULE_MAGIC 0x444F534D   /* "DOSM" */

// Pass HAL Functions to module main entry.
typedef struct {
    void (*print)    (const char*);
    void (*print_hex)(uintptr_t);
    void (*print_dec)(size_t);
    void (*putchar)  (char);
    void (*clear)    (void);
    void (*panic)    (const char*);
} hal_api_t;

/* Binary header of each module */
typedef struct {
    uint32_t       magic;          /* M ODULE_MAGIC */
    char           name[32];       /* module name */
    char           version[16];    /* module version */
    uint32_t       reserved;
} __attribute__((packed)) module_header_t;

/* main entrance of each module of multiboot2 */
typedef void (*module_entry_fn)(const hal_api_t* hal);

#endif