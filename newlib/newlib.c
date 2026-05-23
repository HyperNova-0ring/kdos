/*
 * newlib.c — KST-backed syscall hooks for newlib.
 * This is the OS-specific glue layer: newlib's libc calls these _* functions,
 * which forward to the kernel via the KST pointer set by crt.S at module entry.
 */

#include "../kernel/module_abi.h"

#include <sys/stat.h>
#include <sys/times.h>
#include <errno.h>
#include <stddef.h>

/* Written by crt.S before main() is called. */
const kst_t* _kst;


/*
 * This newlib version (4.x) uses the reentrant model: libc internals call
 * the non-prefixed names (write, read, sbrk, ...) via _write_r, _read_r, ...
 * We provide those non-prefixed names here. _exit is the only exception —
 * it is still called directly as _exit by libc's exit().
 */

/* ── I/O ─────────────────────────────────────────────────────── */

int write(int fd, const void* buf, size_t len) {
    if (!_kst) { errno = ENOSYS; return -1; }
    int r = _kst->io.write(fd, buf, len);
    if (r < 0) { errno = EIO; return -1; }
    return r;
}

int read(int fd, void* buf, size_t len) {
    if (!_kst) { errno = ENOSYS; return -1; }
    int r = _kst->io.read(fd, buf, len);
    if (r < 0) { errno = EIO; return -1; }
    return r;
}

int open(const char* path, int flags, int mode) {
    if (!_kst) { errno = ENOSYS; return -1; }
    int r = _kst->io.open(path, flags, mode);
    if (r < 0) { errno = ENOENT; return -1; }
    return r;
}

int close(int fd) {
    if (!_kst) { errno = ENOSYS; return -1; }
    int r = _kst->io.close(fd);
    if (r < 0) { errno = EBADF; return -1; }
    return r;
}

int isatty(int fd) {
    if (!_kst) return 0;
    return _kst->io.isatty(fd);
}

int lseek(int fd, int offset, int whence) {
    if (!_kst) { errno = ENOSYS; return -1; }
    int r = _kst->io.lseek(fd, offset, whence);
    if (r < 0) { errno = ESPIPE; return -1; }
    return r;
}

int fstat(int fd, struct stat* st) {
    if (!_kst) { errno = ENOSYS; return -1; }
    return _kst->io.fstat(fd, st);
}

/* ── Memory ──────────────────────────────────────────────────── */

void* sbrk(ptrdiff_t incr) {
    if (!_kst) return (void*)-1;
    void* p = _kst->mem.sbrk((intptr_t)incr);
    if (p == (void*)-1) errno = ENOMEM;
    return p;
}

/* ── Process ─────────────────────────────────────────────────── */

int getpid(void) {
    if (!_kst) return 1;
    return _kst->sys.getpid();
}

int kill(int pid, int sig) {
    (void)pid;
    /* SIGABRT (6): route to kernel panic so assert/abort gives a readable halt */
    if (sig == 6 && _kst)
        _kst->sys.panic("abort() called from module");
    errno = EINVAL;
    return -1;
}

void _exit(int status) {
    if (_kst) _kst->sys.exit(status);
    for (;;) __asm__ volatile ("hlt");
}
