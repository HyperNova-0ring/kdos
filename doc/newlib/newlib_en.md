# KDOS newlib — Documentation

`newlib/` provides a C runtime environment (CRT) and libc syscall glue that lets you write KDOS programs in standard C. It bridges the newlib libc to the KDOS Kernel Services Table (KST).

## Purpose

KDOS programs are **standard ELF64 executables** (`ET_EXEC`). They are not flat binary modules — they carry no `module_header_t`. They are compiled and linked as normal executables, transferred to the system at runtime by `command_kern`. The CRT and newlib glue let programs be written with `main`, `printf`, `malloc`, and `exit`:

```c
#include <stdio.h>

int main(void) {
    printf("Hello from KDOS!\n");
    return 0;
}
```

The kernel loads them via `kst->sys.elf_load` (the neutral ELF loader) and calls the CRT entry point `_prog_entry`.

## Directory Structure

```
newlib/
├── Makefile
├── newlib.h              — Exposes _kst (KST pointer for direct access)
├── newlib.c              — Syscall stubs (write, read, sbrk, exit, ...)
└── arch/
    └── x86_64/
        ├── crt.S         — CRT startup: _prog_entry() → main()
        └── prog.ld       — Linker script for ELF programs
```

## Build

```bash
make -C newlib ARCH=x86_64
```

### Output

| Artifact                     | Description                              |
|------------------------------|------------------------------------------|
| `build/x86_64/crt.o`         | CRT startup object                       |
| `build/x86_64/libnewlib.a`   | Syscall glue static library              |
| `build/x86_64/prog.ld`       | ELF program linker script                |

### CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -fPIC -fvisibility=hidden -O2
-I../kernel -I. -isystem <newlib>/include
```

`-fPIC` because programs are loaded at arbitrary physical addresses.  
`-fvisibility=hidden` prevents accidental symbol export from the glue layer.

### Toolchain Requirement

The host must have the newlib cross-compiler installed at `/usr/local/x86_64-elf/`. The Makefile references it via `-isystem $(NEWLIB)/include`.

---

## CRT Startup — `arch/x86_64/crt.S`

The kernel ELF loader jumps to the program's entry point (`_prog_entry`) with the KST in `%rdi` (SysV ABI).

### Startup Sequence

1. **Save `kst`** — `%rdi` is clobbered by BSS zeroing; pushed onto the stack first.
2. **Zero BSS** — `rep stosb` from `__bss_start` to `__bss_end` (RIP-relative). Required because ELF segments are copied as-is; BSS is not pre-zeroed.
3. **Store `kst` in `_kst`** — The `_kst` global defined in `newlib.c` is set to the saved pointer.
4. **Fix `_impure_ptr`** — Newlib's reentrant model requires `_impure_ptr` to point to `_impure_data`. Fixed at runtime with RIP-relative addressing.
5. **16-byte stack alignment** — `andq $-16, %rsp` per the SysV AMD64 ABI.
6. **Call `main(0, NULL)`** — `argc = 0`, `argv = NULL`.
7. **Call `_exit(return_value)`** — routes through `kst->sys.exit`; never returns.

After `_exit` there is an unreachable `hlt` loop as a defensive measure.

---

## Syscall Glue — `newlib.c`

Provides the OS-specific syscall implementations that newlib's libc expects.

### Global State

```c
const kst_t* _kst;   // set by crt.S before main() is called
```

Accessible from programs via `#include "newlib.h"` for direct KST calls beyond what the standard C library provides.

### I/O Functions

| Function            | Backed by         | Notes                              |
|---------------------|-------------------|------------------------------------|
| `write(fd, buf, n)` | `kst->io.write`   | fd 1/2 → console; others → EIO    |
| `read(fd, buf, n)`  | `kst->io.read`    | Stub: returns EIO                  |
| `open(path, ...)`   | `kst->io.open`    | Stub: returns ENOENT               |
| `close(fd)`         | `kst->io.close`   | Stub: returns EBADF                |
| `isatty(fd)`        | `kst->io.isatty`  | fd 0–2 returns 1                   |
| `lseek(fd, ...)`    | `kst->io.lseek`   | Stub: returns ESPIPE               |
| `fstat(fd, st)`     | `kst->io.fstat`   | Stub                               |

### Memory

```c
void* sbrk(ptrdiff_t incr);   // → kst->mem.sbrk → heap_sbrk in kernel
```

Enables `malloc`/`free` since newlib's allocator uses `sbrk` internally. `sbrk(0)` returns the current break (heap state probe by `malloc`).

### Process

```c
int  getpid(void);           // → kst->sys.getpid (stub: 1)
int  kill(int pid, int sig); // SIGABRT → kst->sys.panic; others → EINVAL
void _exit(int status);      // → kst->sys.exit
```

---

## Program Linker Script — `arch/x86_64/prog.ld`

Used when linking ELF programs for KDOS. No `module_header_t` — programs are standard ELF.

```
ENTRY(_prog_entry)
. = 0x2000000;          — base address: 32 MB (above kernel and its modules)

.text   : { *(.text.entry) *(.text .text.*) }
.rodata ALIGN(8) : { ... }
.data   ALIGN(8) : { ... }
.got    ALIGN(8) : { ... }
.got.plt ALIGN(8): { ... }
.bss    ALIGN(8) : {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
}
/DISCARD/ : { .eh_frame .comment .note.* }
```

The critical constraint: `_prog_entry` from `crt.o` must be first in `.text`. The linker guarantees this by placing `.text.entry` before `.text`.

---

## How to Build a Program

```bash
x86_64-elf-gcc \
    -ffreestanding -mno-red-zone -fno-stack-protector \
    -mno-sse -mno-sse2 -mno-mmx -fPIC -O2 \
    -I../kernel -I../newlib \
    -isystem /usr/local/x86_64-elf/include \
    -c main.c -o main.o

x86_64-elf-ld -T build/x86_64/prog.ld -nostdlib \
    build/x86_64/crt.o main.o build/x86_64/libnewlib.a \
    /usr/local/x86_64-elf/lib/libc.a \
    -o myprogram.elf
```

The resulting ELF is a standard ELF64 executable. Transfer it to the system using `command_kern`'s KELF protocol (see `doc/command_kern/`).

Link order matters: `crt.o` must come before `main.o`.

---

## Relationship with the System

```
command_kern (load)
  └─ transfer_recv_elf()    — receive ELF over COM2 (KELF protocol)
       └─ kst->sys.elf_load(buf, size)
            └─ kernel/elf.c: load PT_LOAD segments into identity-mapped memory
                 └─ returns e_entry (_prog_entry in crt.o)
                      └─ crt.S: zeros BSS, sets _kst, fixes _impure_ptr, calls main()
                           └─ main() uses printf/malloc/exit
                                └─ newlib libc → write/sbrk/_exit in newlib.c
                                     └─ newlib.c → kst->io.write / kst->mem.sbrk / kst->sys.exit
                                          └─ KST → HAL (console, heap, panic)
```
