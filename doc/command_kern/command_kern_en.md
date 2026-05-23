# KDOS command_kern — Documentation

`command_kern/` is the primary entry-point module of KDOS, analogous to `COMMAND.COM` in MS-DOS. After the kernel has finished its own initialization and run all auxiliary modules, it searches for this module by name and hands execution to it. It is the first interactive layer of the OS.

## Directory Structure

```
command_kern/
├── Makefile
├── main.c       — Module source code
└── module.ld    — Module linker script
```

## Current State

The module is a minimal skeleton: it declares the module header as `COMMAND.KERN` version `1.0` and its entry function simply prints `"Hello World!\n"` via the KST console. It does not yet implement any interactive shell functionality.

```c
#include "../kernel/module_abi.h"

MODULE_HEADER("COMMAND.KERN", "1.0");

__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    kst->console.print("Hello World!\n");
}
```

---

## Build

```bash
# From repo root:
make              # builds command.kern.bin as part of all

# From command_kern/ directly:
make -C command_kern ARCH=x86_64
```

### Output

| Artifact                              | Description                     |
|---------------------------------------|---------------------------------|
| `build/x86_64/command.kern.elf`       | ELF intermediate                |
| `build/x86_64/command.kern.bin`       | Final flat binary               |

The root Makefile copies `command.kern.bin` to `build/x86_64/modules/` and then into the ISO's `modules/` directory.

### Compiler Flags

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -fPIC -O2
-falign-functions=1 -falign-loops=1 -falign-jumps=1
-I../kernel
```

`-fPIC` is required because the module is loaded at an arbitrary physical address.  
The tight alignment flags (`-falign-*=1`) minimize dead bytes between functions in the flat binary.

---

## Linker Script — `module.ld`

```
. = 0;
.module_header : { KEEP(*(.module_header)) }
.text ALIGN(1) : { *(.text.entry) *(.text .text.*) }
.rodata ALIGN(1)
.data   ALIGN(1)
.got    ALIGN(1)
.got.plt ALIGN(1)
.bss    ALIGN(1)
/DISCARD/ : { .eh_frame, .comment, .note.* }
```

Identical in structure to `modules/hello/module.ld`. Module loads at virtual address 0; the kernel reads the header at offset 0 and calls the entry at offset 56 (`sizeof(module_header_t)`).

---

## How the Kernel Launches It

`modules_launch_entry()` in `kernel/modules.c` searches the module list for a module named `COMMAND.KERN` (checked against both cmdline and `module_header_t.name`). When found:

1. Prints `"Launching COMMAND.KERN..."`.
2. Computes entry address: `module_start + sizeof(module_header_t)` = `module_start + 56`.
3. Casts entry to `module_entry_fn` and calls it with `&kernel_kst`.
4. The module runs; when it returns, `modules_launch_entry` returns and `kernel_main` calls `hal_panic("kernel_main end reached.")`.

There is currently no mechanism for the shell to yield back to the kernel gracefully. A return from `module_main` is considered an error.

---

## ABI Used

`command_kern` uses `module_abi.h` directly (no CRT, no newlib). The entry function follows the bare module ABI:

```c
typedef void (*module_entry_fn)(const kst_t* kst);
```

To use `printf`, `malloc`, or other libc functions, the module would need to be rebuilt with the CRT + newlib from `newlib/`. See `doc/newlib/newlib_en.md` for how to do this.

---

## GRUB Configuration

The root Makefile auto-generates `grub.cfg` during `make iso`. For each `.bin` file in `build/x86_64/iso/modules/`, it adds:

```
module2 /modules/command.kern.bin command.kern.bin
```

GRUB passes the module's filename as its command line. The kernel stores this in `module_t.cmdline`. The module is also identified by `module_header_t.name = "COMMAND.KERN"`.

---

## Planned Functionality

As KDOS evolves, `command_kern` is intended to become a full interactive command interpreter with:

- Reading keystrokes via `kst->console.getchar()` (currently a stub returning -1, awaiting keyboard driver)
- Parsing and dispatching built-in commands
- Loading and executing additional modules
- A basic filesystem interface once storage drivers are implemented

This makes `command_kern` the primary place where future OS functionality will be exercised.
