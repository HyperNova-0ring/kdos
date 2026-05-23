# KDOS newlib — Documentation

`newlib/` provides a C runtime environment (CRT) and libc syscall glue that allows modules to use standard C library functions such as `printf`, `malloc`, and `exit`. It bridges the newlib libc to the KDOS Kernel Services Table (KST).

## Purpose

Without this layer a module must call KST functions directly (`kst->console.print(...)`, etc.). With newlib linked in, the module can write standard C:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Hello from a standard C module!\n");
    return 0;
}
```

## Directory Structure

```
newlib/
├── Makefile
├── newlib.h              — MODULE_DEFINE macro
├── newlib.c              — Syscall stubs (write, read, sbrk, exit, ...)
└── arch/
    └── x86_64/
        ├── crt.S         — CRT startup: _module_entry() → main()
        └── module.ld     — Linker script for CRT-based modules
```

## Build

```bash
make -C newlib ARCH=x86_64
```

### Output

| Artifact             | Description                                     |
|----------------------|-------------------------------------------------|
| `build/x86_64/crt.o`       | CRT startup object                        |
| `build/x86_64/libnewlib.a` | Syscall glue static library               |
| `build/x86_64/module.ld`   | Module linker script (copy to build root) |

These are copied to `build/x86_64/` by the root Makefile.

### CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -fPIC -fvisibility=hidden -O2
-I../kernel -I. -isystem <newlib>/include
```

`-fPIC` is required because modules load at arbitrary physical addresses.  
`-fvisibility=hidden` prevents accidental symbol export from the glue layer.

### Toolchain Requirement

The host system must have a newlib cross-compiler installed at `/usr/local/x86_64-elf/`. The Makefile references it via `-isystem $(NEWLIB)/include` for header inclusion and implicitly for linking `libc.a` when building a full module.

---

## CRT Startup — `arch/x86_64/crt.S`

The kernel calls every module as:

```c
void _module_entry(const kst_t* kst);   // kst in %rdi (SysV ABI)
```

`crt.S` implements `_module_entry` and performs the following steps before calling `main`:

### Step-by-step

1. **Save `kst`** — `%rdi` will be overwritten by the BSS zeroing loop, so it is pushed onto the stack.

2. **Zero BSS** — Using `rep stosb` from `__bss_start` to `__bss_end`. This is required because modules are raw binaries loaded at arbitrary addresses; their BSS is not pre-zeroed by the OS.

3. **Store `kst` in `_kst`** — The now-zeroed `_kst` global (defined in `newlib.c`) is set to the saved `kst` pointer using RIP-relative addressing.

4. **Fix `_impure_ptr`** — Newlib's reentrant model requires `_impure_ptr` to point to the actual `_impure_data` struct. Since the module is PIC and loads at address 0-relative layout, this pointer is fixed up at runtime.

5. **16-byte stack alignment** — `andq $-16, %rsp` satisfies the SysV AMD64 ABI requirement before any call instruction.

6. **Call `main(0, NULL)`** — `argc = 0`, `argv = NULL`.

7. **Call `_exit(main_return_value)`** — never returns; routes through `kst->sys.exit`.

### Safety Halt

After `_exit` there is an unreachable `hlt` loop as a defensive measure.

---

## Syscall Glue — `newlib.c`

This file provides the OS-specific syscall implementations that newlib's libc expects. Newlib 4.x uses the reentrant model internally (`_write_r`, `_read_r`, etc.) but still expects the non-prefixed names to be defined by the OS layer.

### Global State

```c
const kst_t* _kst;   // set by crt.S before main() is called
```

All functions guard against `_kst == NULL` and return `ENOSYS` if called before the KST is set.

### I/O Functions

| Function            | Backed by           | Notes                              |
|---------------------|---------------------|------------------------------------|
| `write(fd, buf, n)` | `kst->io.write`     | fd 1/2 → VGA; others return EIO   |
| `read(fd, buf, n)`  | `kst->io.read`      | Stub: returns EIO                 |
| `open(path, ...)`   | `kst->io.open`      | Stub: returns ENOENT              |
| `close(fd)`         | `kst->io.close`     | Stub: returns EBADF               |
| `isatty(fd)`        | `kst->io.isatty`    | fd 0–2 returns 1                  |
| `lseek(fd, ...)`    | `kst->io.lseek`     | Stub: returns ESPIPE              |
| `fstat(fd, st)`     | `kst->io.fstat`     | Stub                              |

### Memory

```c
void* sbrk(ptrdiff_t incr);   // → kst->mem.sbrk → heap_sbrk in kernel
```

This enables `malloc`/`free` in the module since newlib's allocator uses `sbrk` internally.

### Process

```c
int  getpid(void);           // → kst->sys.getpid (stub: 1)
int  kill(int pid, int sig); // SIGABRT (6) → kst->sys.panic; others → EINVAL
void _exit(int status);      // → kst->sys.exit → kernel panic
```

---

## Module Header Macro — `newlib.h`

```c
#define MODULE_DEFINE(name_str, version_str)
```

Embeds a `module_header_t` at file scope using `__attribute__((section(".module_header"), used))`. Use this macro instead of `MODULE_HEADER` from `module_abi.h` when building with the CRT.

Example:

```c
MODULE_DEFINE("myapp", "1.0");

int main(void) {
    printf("Hello!\n");
    return 0;
}
```

---

## Module Linker Script — `arch/x86_64/module.ld`

Used when linking modules with the CRT. Layout at offset 0:

```
. = 0;
.module_header          — MODULE_DEFINE / MODULE_HEADER macro output (56 bytes)
.text ALIGN(1):
    *(.text.entry)      — crt.o: _module_entry must be FIRST in .text
    *(.text .text.*)    — module code
.rodata ALIGN(1)
.data   ALIGN(1)
.got    ALIGN(1)        — required for PIC
.got.plt ALIGN(1)
.bss    ALIGN(1):       — __bss_start … __bss_end symbols (used by crt.S)
/DISCARD/: .eh_frame, .comment, .note.*
```

The critical constraint: `_module_entry` from `crt.o` must be at offset `sizeof(module_header_t)` (56). The linker achieves this by placing `.text.entry` first in `.text`, immediately after `.module_header`.

---

## How to Build a Module with CRT + Newlib

```bash
x86_64-elf-gcc \
    -ffreestanding -mno-red-zone -fno-stack-protector \
    -mno-sse -mno-sse2 -mno-mmx -fPIC -O2 \
    -I../kernel -I../newlib \
    -isystem /usr/local/x86_64-elf/include \
    -c main.c -o main.o

x86_64-elf-ld -T build/x86_64/module.ld -nostdlib \
    build/x86_64/crt.o main.o build/x86_64/libnewlib.a \
    /usr/local/x86_64-elf/lib/libc.a \
    -o mymodule.elf

x86_64-elf-objcopy -O binary mymodule.elf mymodule.bin
```

The link order matters: `crt.o` must come before `main.o` so that `_module_entry` ends up first in `.text.entry`.

---

## Relationship with the Kernel

```
Kernel
  └─ modules_run_all() / modules_launch_entry()
       └─ calls entry = module_start + 56  (= _module_entry in crt.o)
            └─ crt.S: zeros BSS, sets _kst, fixes _impure_ptr, calls main()
                 └─ module's main() uses printf/malloc/exit
                      └─ newlib libc → write/sbrk/_exit in newlib.c
                           └─ newlib.c → kst->io.write / kst->mem.sbrk / kst->sys.exit
                                └─ KST → HAL (VGA, heap, panic)
```
