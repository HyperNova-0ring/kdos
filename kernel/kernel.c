#include "hal.h"
#include "multiboot2.h"
#include "modules.h"

static void parse_memory_map(void* mb2_info) {
    mb2_info_t* info = (mb2_info_t*)mb2_info;
    mb2_tag_t*  tag  = (mb2_tag_t*)((uint8_t*)info + 8);

    hal_mem_region_t regions[64];
    uint32_t         count = 0;

    while (tag->type != MB2_TAG_END) {
        if (tag->type == MB2_TAG_MMAP) {
            mb2_tag_mmap_t*  mmap  = (mb2_tag_mmap_t*)tag;
            mb2_mmap_entry_t* entry = (mb2_mmap_entry_t*)
                                      ((uint8_t*)mmap + sizeof(*mmap));
            mb2_mmap_entry_t* end  = (mb2_mmap_entry_t*)
                                      ((uint8_t*)mmap + mmap->size);

            while (entry < end && count < 64) {
                regions[count].base   = entry->base_addr;
                regions[count].length = entry->length;
                regions[count].type   = entry->type;
                count++;
                entry = (mb2_mmap_entry_t*)
                        ((uint8_t*)entry + mmap->entry_size);
            }
        }
        tag = MB2_TAG_NEXT(tag);
    }

    hal_mem_set_map(regions, count);
}

void kernel_main(uint64_t mb2_magic, uint64_t mb2_addr) {
    hal_console_init();
    hal_console_clear();

    hal_console_print("KDOS Kernel\n");
    hal_console_print("---------------\n\n");

    hal_console_print("Multiboot2 resides in direction:");
    hal_console_print_hex(mb2_addr);
    hal_console_print("\nwith value:");
    hal_console_print_hex(mb2_magic);
    // Verificar magic multiboot2
    if (mb2_magic != MULTIBOOT2_MAGIC) {
        hal_panic("Multiboot2 magic is invalid");
    } else {
        hal_console_print("\n Multiboot2 magic found\n");
    }

    // Parsear mapa de memoria
    parse_memory_map((void*)mb2_addr);

    hal_mem_region_t map[64];
    uint32_t n = hal_mem_get_map(map, 64);

    hal_console_print("Memory detected:\n");
    for (uint32_t i = 0; i < n; i++) {
        hal_console_print("  ");
        hal_console_print_hex(map[i].base);
        hal_console_print(" + ");
        hal_console_print_hex(map[i].length);
        hal_console_print(map[i].type == 1 ? "  [usable]\n"
                                            : "  [reserved]\n");
    }

    // Cargar módulos
    modules_init((void*)mb2_addr);
    uint32_t mod_count = modules_count();

    hal_console_print("\nLoaded modules: ");
    hal_console_print_dec(mod_count);
    hal_console_print("\n");

#ifdef __DEBUG__
    module_list_t* mods = modules_get_all();
    for (uint32_t i = 0; i < mods->count; i++) {
        hal_console_print("  [");
        hal_console_print_dec(i);
        hal_console_print("] ");
        hal_console_print(mods->modules[i].name);
        hal_console_print(" @ ");
        hal_console_print_hex(mods->modules[i].start);
        hal_console_print("\n");
    }
#endif

    modules_run_all();

    // TODO: Fix this.
    module_t* cmd = modules_find("command.com");
    if (cmd) {
        hal_console_print("\nLoading command.com...\n");
        // TODO: ejecutar el módulo
        void (*entry)(void) = (void(*)(void))(uint64_t)cmd->start;
        entry();
	hal_console_print("\n COMMAND.COM exit, going kernel panic...");
    } else {
        hal_console_print("\nCan't find a command.com executable...\n");
    }

    hal_panic("kernel_main end reached.");
}
