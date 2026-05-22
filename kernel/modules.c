#include "modules.h"
#include "hal.h"

static module_list_t module_list;

static hal_api_t kernel_hal = {
    .print     = hal_console_print,
    .print_hex = hal_console_print_hex,
    .print_dec = hal_console_print_dec,
    .putchar   = hal_console_putchar,
    .clear     = hal_console_clear,
    .panic     = hal_panic,
};

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

void modules_register(uintptr_t start, uintptr_t end, const char* cmdline) {
    if (module_list.count >= MAX_MODULES) return;

    uint32_t i  = module_list.count;
    module_t *m = &module_list.modules[i];

    m->start = start;
    m->end   = end;
    m->state = MODULE_STATE_LOADED;
    str_copy(m->cmdline, cmdline, 64);

    module_header_t *hdr = (module_header_t *)start;
    if (hdr->magic == MODULE_MAGIC) {
        m->header = hdr;
    } else {
        m->header = NULL;
        m->state  = MODULE_STATE_FAILED;
        hal_console_print("  [WARN] ");
        hal_console_print(cmdline);
        hal_console_print(": missing module header\n");
    }

    module_list.count++;
}

void modules_run_all(void) {
    for (uint32_t i = 0; i < module_list.count; i++) {
        module_t *m = &module_list.modules[i];

        if (m->state != MODULE_STATE_LOADED || m->header == NULL)
            continue;

        hal_console_print("  [MOD] ");
        hal_console_print(m->header->name);
        hal_console_print(" v");
        hal_console_print(m->header->version);
        hal_console_print("\n");

        module_entry_fn entry = (module_entry_fn)
            (m->start + sizeof(module_header_t));

        m->state = MODULE_STATE_RUNNING;
        entry(&kernel_hal);
    }
}

uint32_t modules_count(void) {
    return module_list.count;
}

module_t* modules_find(const char* name) {
    for (uint32_t i = 0; i < module_list.count; i++) {
        if (str_eq(module_list.modules[i].cmdline, name))
            return &module_list.modules[i];
    }
    return NULL;
}

module_list_t* modules_get_all(void) {
    return &module_list;
}
