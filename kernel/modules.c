#include "modules.h"
#include "multiboot2.h"

static module_list_t module_list;

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
                module_list.modules[i].start  = mod->mod_start;
                module_list.modules[i].end    = mod->mod_end;
                module_list.modules[i].loaded = 1;
                str_copy(module_list.modules[i].name,
                         mod->cmdline, 64);
                module_list.count++;
            }
        }
        tag = MB2_TAG_NEXT(tag);
    }
}

uint32_t modules_count(void) {
    return module_list.count;
}

module_t* modules_find(const char* name) {
    for (uint32_t i = 0; i < module_list.count; i++) {
        if (str_eq(module_list.modules[i].name, name))
            return &module_list.modules[i];
    }
    return 0;
}

module_list_t* modules_get_all(void) {
    return &module_list;
}