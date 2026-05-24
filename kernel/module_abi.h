#ifndef MODULE_ABI_H
#define MODULE_ABI_H

#include <stdint.h>
#include <stddef.h>

#define MODULE_MAGIC  0x444F534D   /* "DOSM" */
#define KST_VERSION   2

typedef struct {
    uint32_t version;

    /* ── Console ─────────────────────────────────────────── */
    struct {
        void (*print)    (const char* str);
        void (*print_hex)(uintptr_t val);
        void (*print_dec)(size_t val);
        void (*putchar)  (char c);
        void (*clear)    (void);
        int  (*getchar)  (void);
    } console;

    /* ── Memory ──────────────────────────────────────────── */
    struct {
        void* (*sbrk)(intptr_t incr);
    } mem;

    /* ── I/O ─────────────────────────────────────────────── */
    struct {
        int (*write) (int fd, const void* buf, size_t len);
        int (*read)  (int fd, void* buf, size_t len);
        int (*open)  (const char* path, int flags, int mode);
        int (*close) (int fd);
        int (*isatty)(int fd);
        int (*lseek) (int fd, int offset, int whence);
        int (*fstat) (int fd, void* st);
    } io;

    /* ── System ──────────────────────────────────────────── */
    struct {
        void (*exit)          (int status);
        void (*panic)         (const char* msg);
        int  (*getpid)        (void);
        /* Program-level exception hook. fn fills *out_rip/*out_rsp with
           the recovery address; IRETQ redirects there. Auto-cleared on entry. */
        void (*set_exc_hook)  (void (*fn)(uint64_t vec, uint64_t rip, uint64_t err,
                                          uint64_t* out_rip, uint64_t* out_rsp));
        void (*clear_exc_hook)(void);
        /* ELF loader — load a standard ELF64 executable into identity-mapped
           memory and return its entry-point address, or 0 on error.
           Exposed here so any module can launch programs without embedding
           its own loader. */
        uintptr_t (*elf_load) (const void* data, size_t size);
    } sys;

} kst_t;

#define MODULE_HEADER(name_str, version_str)                            \
    __attribute__((section(".module_header"), used))                    \
    static const module_header_t _module_hdr = {                       \
        .magic    = MODULE_MAGIC,                                       \
        .name     = name_str,                                           \
        .version  = version_str,                                        \
        .reserved = 0,                                                  \
    }

typedef struct {
    uint32_t magic;
    char     name[32];
    char     version[16];
    uint32_t reserved;
} __attribute__((packed)) module_header_t;

typedef void (*module_entry_fn)(const kst_t* kst);

#endif
