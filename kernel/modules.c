#include "modules.h"
#include "hal.h"
#include "heap.h"

static module_list_t module_list;

/* ── KST implementations ─────────────────────────────────── */

static int kst_write(int fd, const void* buf, size_t len) {
    if (fd == 1 || fd == 2) {
        const char* s = (const char*)buf;
        for (size_t i = 0; i < len; i++)
            hal_console_putchar(s[i]);
        return (int)len;
    }
    return -1;
}

static int kst_read(int fd, void* buf, size_t len) {
    if (fd != 0 || len == 0)
        return -1;
    char* dst = (char*)buf;
    for (size_t i = 0; i < len; i++) {
        int c = hal_console_getchar();
        dst[i] = (char)c;
        if (c == '\n') return (int)(i + 1);
    }
    return (int)len;
}

static int kst_open(const char* path, int flags, int mode) {
    (void)path; (void)flags; (void)mode;
    return -1;
}

static int kst_close(int fd) {
    (void)fd;
    return -1;
}

static int kst_isatty(int fd) {
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

static int kst_lseek(int fd, int offset, int whence) {
    (void)fd; (void)offset; (void)whence;
    return -1;
}

static int kst_fstat(int fd, void* st) {
    (void)fd; (void)st;
    return -1;
}

static void kst_exit(int status) {
    (void)status;
    hal_panic("module called exit()");
}

static int kst_getpid(void) {
    return 1;
}

/* ── Kernel Services Table ───────────────────────────────── */

static kst_t kernel_kst = {
    .version = KST_VERSION,
    .console = {
        .print     = hal_console_print,
        .print_hex = hal_console_print_hex,
        .print_dec = hal_console_print_dec,
        .putchar   = hal_console_putchar,
        .clear     = hal_console_clear,
        .getchar   = hal_console_getchar,
    },
    .mem = {
        .sbrk = heap_sbrk,
    },
    .io = {
        .write  = kst_write,
        .read   = kst_read,
        .open   = kst_open,
        .close  = kst_close,
        .isatty = kst_isatty,
        .lseek  = kst_lseek,
        .fstat  = kst_fstat,
    },
    .sys = {
        .exit   = kst_exit,
        .panic  = hal_panic,
        .getpid = kst_getpid,
    },
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

static int module_name_eq(const module_t* m, const char* name) {
    if (str_eq(m->cmdline, name)) return 1;
    if (m->header && str_eq(m->header->name, name)) return 1;
    return 0;
}

static int module_is_entry_shell(const module_t* m) {
    return module_name_eq(m, "COMMAND.KERN") || module_name_eq(m, "command.com");
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
        if (module_is_entry_shell(m))
            continue;

        hal_console_print("  [MOD] ");
        hal_console_print(m->header->name);
        hal_console_print(" v");
        hal_console_print(m->header->version);
        hal_console_print("\n");

        module_entry_fn entry = (module_entry_fn)
            (m->start + sizeof(module_header_t));

        m->state = MODULE_STATE_RUNNING;
        entry(&kernel_kst);
    }
}

void modules_launch_entry(void) {
    const char* candidates[] = { "COMMAND.KERN", "command.com", NULL };

    for (uint32_t i = 0; candidates[i]; i++) {
        module_t *m = modules_find(candidates[i]);
        if (!m || m->header == NULL)
            continue;

        hal_console_print("\nLaunching ");
        hal_console_print(m->header->name);
        hal_console_print("...\n");

        module_entry_fn entry = (module_entry_fn)
            (m->start + sizeof(module_header_t));

        m->state = MODULE_STATE_RUNNING;
        entry(&kernel_kst);
        return;
    }

    hal_console_print("No entry executable found (COMMAND.KERN or command.com).\n");
}

uint32_t modules_count(void) {
    return module_list.count;
}

module_t* modules_find(const char* name) {
    for (uint32_t i = 0; i < module_list.count; i++) {
        if (module_name_eq(&module_list.modules[i], name))
            return &module_list.modules[i];
    }
    return NULL;
}

module_list_t* modules_get_all(void) {
    return &module_list;
}
