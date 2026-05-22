#include "hal.h"
#include "modules.h"
#include "heap.h"

extern uintptr_t kernel_end;

void kernel_main(uint64_t boot_magic, uint64_t boot_addr) {
    hal_console_init();
    hal_console_clear();
    hal_idt_init();

    hal_console_print("KDOS Kernel\n");
    hal_console_print("---------------\n\n");

    hal_arch_init(boot_magic, boot_addr);
    hal_mem_init();
    heap_init((uintptr_t)&kernel_end);

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

    uint32_t mod_count = modules_count();
    hal_console_print("\nLoaded modules: ");
    hal_console_print_dec(mod_count);
    hal_console_print("\n");

#ifdef __DEBUG__
    module_list_t *mods = modules_get_all();
    for (uint32_t i = 0; i < mods->count; i++) {
        module_t *m = &mods->modules[i];
        hal_console_print("  [");
        hal_console_print_dec(i);
        hal_console_print("] ");
        hal_console_print(m->header ? m->header->name : m->cmdline);
        hal_console_print(" @ ");
        hal_console_print_hex(m->start);
        hal_console_print("\n");
    }
#endif

    modules_run_all();

    // TODO: Fix this.
    module_t *cmd = modules_find("command.com");
    if (cmd) {
        hal_console_print("\nLoading command.com...\n");
        void (*entry)(void) = (void(*)(void))cmd->start;
        entry();
        hal_console_print("\n COMMAND.COM exit, going kernel panic...");
    } else {
        hal_console_print("\nCan't find a command.com executable...\n");
    }

    hal_panic("kernel_main end reached.");
}
