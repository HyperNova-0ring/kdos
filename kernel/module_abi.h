#ifndef MODULE_ABI_H
#define MODULE_ABI_H

#include <stdint.h>
#include <stddef.h>

#define MODULE_MAGIC  0x444F534D   /* "DOSM" */
#define KST_VERSION   1

/*
 * Kernel Services Table — contract between the kernel and any module or
 * program running at ring 0. Replaces syscalls; passed by pointer to
 * every module entry point.
 *
 * Fields marked as stubs return safe values until the underlying
 * subsystem is implemented.
 */
typedef struct {
    uint32_t version;               /* KST_VERSION */

    /* ── Console ─────────────────────────────────────────── */
    struct {
        void (*print)    (const char* str);
        void (*print_hex)(uintptr_t val);
        void (*print_dec)(size_t val);
        void (*putchar)  (char c);
        void (*clear)    (void);
        int  (*getchar)  (void);    /* stub: -1 until keyboard driver is ready */
    } console;

    /* ── Memory ──────────────────────────────────────────── */
    struct {
        void* (*sbrk)(intptr_t incr);   /* stub: (void*)-1 until PMM */
    } mem;

    /* ── I/O (newlib stubs: _read/_write/_open/_close ...) ── */
    struct {
        int (*write) (int fd, const void* buf, size_t len);
        int (*read)  (int fd, void* buf, size_t len);
        int (*open)  (const char* path, int flags, int mode);
        int (*close) (int fd);
        int (*isatty)(int fd);
        int (*lseek) (int fd, int offset, int whence);
        int (*fstat) (int fd, void* st);    /* struct stat* — type TBD */
    } io;

    /* ── System ─────────────────────────────────────────── */
    struct {
        void (*exit) (int status);
        void (*panic)(const char* msg);
        int  (*getpid)(void);
    } sys;

} kst_t;

/* ── Module binary layout ────────────────────────────────── */

typedef struct {
    uint32_t magic;         /* MODULE_MAGIC */
    char     name[32];
    char     version[16];
    uint32_t reserved;
} __attribute__((packed)) module_header_t;

typedef void (*module_entry_fn)(const kst_t* kst);

#endif
