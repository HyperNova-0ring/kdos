#include "modules.h"
#include "multiboot2.h"

static module_list_t module_list;

static hal_api_t kernel_hal = {
    .print     = hal_console_print,
    .print_hex = hal_console_print_hex,
    .print_dec = hal_console_print_dec,
    .putchar   = hal_console_putchar,
    .clear     = hal_console_clear,
    .panic     = hal_panic,
};

// Copiar string sin stdlib
static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

void modules_init(void* mb2_info) {
    module_list.count = 0;

    mb2_info_t* info = (mb2_info_t*)mb2_info;
    mb2_tag_t*  tag  = (mb2_tag_t*)((uint8_t*)info + 8);

    while (tag->type != MB2_TAG_END) {
        if (tag->type == MB2_TAG_MODULE) {
            mb2_tag_module_t* mod = (mb2_tag_module_t*)tag;
            uint32_t i = module_list.count;

            if (i < MAX_MODULES) {
                module_t* m = &module_list.modules[i];

                m->start = (uintptr_t)mod->mod_start;
                m->end   = (uintptr_t)mod->mod_end;
                m->state = MODULE_STATE_LOADED;
                str_copy(m->cmdline, mod->cmdline, 64);

                /* Verificar si el binario tiene el header válido */
                module_header_t* hdr = (module_header_t*)m->start;
                if (hdr->magic == MODULE_MAGIC) {
                    m->header = hdr;
                } else {
                    m->header = NULL;
                    m->state  = MODULE_STATE_FAILED;
                    hal_console_print("  [WARN] ");
                    hal_console_print(m->cmdline);
                    hal_console_print(": missing module header\n");
                }

                module_list.count++;
            }
        }
        tag = MB2_TAG_NEXT(tag);
    }
}
/*
void modules_run_all(void) {
    for (uint32_t i = 0; i < module_list.count; i++) {
        module_t* m = &module_list.modules[i];

        if (m->state != MODULE_STATE_LOADED || m->header == NULL)
            continue;

        hal_console_print("  [MOD] ");
        hal_console_print(m->header->name);
        hal_console_print(" v");
        hal_console_print(m->header->version);
        hal_console_print("\n");
        hal_console_print("  entry offset: ");
        hal_console_print_dec(m->header->entry_offset);
        hal_console_print("\n");

        // Entry point = justo despues del header
        module_entry_fn entry = (module_entry_fn)
            (m->start + sizeof(module_header_t));

        m->state = MODULE_STATE_RUNNING;
        entry(&kernel_hal);
    }
} */

void modules_run_all(void) {
    for (uint32_t i = 0; i < module_list.count; i++) {
        module_t* m = &module_list.modules[i];

        if (m->state != MODULE_STATE_LOADED || m->header == NULL)
            continue;

        hal_console_print("  [MOD] ");
        hal_console_print(m->header->name);
        hal_console_print(" v");
        hal_console_print(m->header->version);
        hal_console_print("\n");

        /* Debug: mostrar direcciones */
        hal_console_print("  mod_start:    ");
        hal_console_print_hex(m->start);
        hal_console_print("\n");

        hal_console_print("  header_size:  ");
        hal_console_print_dec(sizeof(module_header_t));
        hal_console_print("\n");

        hal_console_print("  entry_addr:   ");
        hal_console_print_hex(m->start + sizeof(module_header_t));
        hal_console_print("\n");

        /* Mostrar los primeros bytes en el punto de entrada */
        uint8_t* entry_bytes = (uint8_t*)(m->start + sizeof(module_header_t));
        hal_console_print("  entry_bytes:  ");
        for (int j = 0; j < 8; j++) {
            hal_console_print_hex(entry_bytes[j]);
            hal_console_print(" ");
        }
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