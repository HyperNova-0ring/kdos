# KDOS — Metaproject Overview

KDOS is a bare-metal DOS-like operating system written in C and GAS assembly. It boots via GRUB/Multiboot2, transitions from 32-bit protected mode to 64-bit long mode, and runs all subsystems as flat binary modules at ring 0.

The end goal is an interactive environment analogous to MS-DOS: a command interpreter (COMMAND.KERN) that launches standard ELF programs using the C library (newlib) as the kernel access layer.

---

## Design Philosophy

| Principle | Description |
|-----------|-------------|
| **No syscalls** | The kernel communicates with modules via the KST (Kernel Services Table), a struct of function pointers passed directly to each entry point. |
| **Ring 0 only** | All code runs at privilege level 0. No user space yet. |
| **Neutral kernel + arch layer** | `kernel/*.c/h` is portable, architecture-independent code. It sees only `hal.h`. Arch-specific detail lives in `kernel/arch/{arch}/`. |
| **ELF as kernel service** | The ELF loader is part of the neutral kernel and exposed to modules via `kst->sys.elf_load`. No module needs to implement its own loader. |
| **Newlib as C standard layer** | Programs are written in standard C (`main`, `printf`, `malloc`). The CRT and newlib glue connect the libc to the KST automatically. |
| **Identity mapping** | Physical address == virtual address for 0–128 MB. The KST is a pointer to a static `kst_t` in the kernel that never moves: accessing the KST is as simple as dereferencing a pointer. |

---

## Repository Structure

```
kdos/
├── Makefile              — Top-level build orchestrator
├── kernel/               — Core kernel
│   ├── *.c / *.h         — Neutral portable layer (hal.h, elf, heap, modules)
│   └── arch/x86_64/      — x86_64-specific implementation
├── newlib/               — CRT + libc glue for programs
├── command_kern/         — Primary shell module (COMMAND.KERN)
├── test/                 — Example programs loaded via `load`
├── build/                — Generated artifacts
└── doc/                  — Documentation
```

---

## System Layers

```
┌─────────────────────────────────────────────────────────┐
│              ELF Programs (main + newlib)                │
│        printf / malloc / read / write → KST             │
├─────────────────────────────────────────────────────────┤
│                  COMMAND.KERN                           │
│    readline · dispatch · load (kst->sys.elf_load)       │
├─────────────────────────────────────────────────────────┤
│       Neutral Kernel  (kernel/*.c)                      │
│  modules · heap · elf_load · KST (kst_t)               │
├─────────────────────────────────────────────────────────┤
│       HAL  (kernel/hal.h ↔ arch/x86_64/hal.c)          │
│  console · PMM · CPU · IDT · panic                      │
├─────────────────────────────────────────────────────────┤
│   Hardware  (VGA · Serial · Keyboard · PMM · IDT)       │
└─────────────────────────────────────────────────────────┘
```

---

## Subprojects

| Subproject       | Role                                                                  |
|------------------|-----------------------------------------------------------------------|
| `kernel/`        | Boot, HAL, PMM, heap, IDT+TSS, ELF loader, module system, KST        |
| `newlib/`        | CRT (`crt.S`), libc syscall glue (`newlib.c`)                         |
| `command_kern/`  | Interactive shell, ELF transfer via COM2 (KELF protocol)             |
| `test/`          | Example programs: standard C and direct KST ASM                       |

---

## Build System

### Toolchain

| Tool                 | Role                         |
|----------------------|------------------------------|
| `x86_64-elf-gcc`     | C compiler (freestanding)    |
| `x86_64-elf-ld`      | Linker                       |
| `x86_64-elf-objcopy` | ELF → raw binary             |
| `x86_64-elf-ar`      | Library archiver             |
| `grub-mkrescue`      | Bootable ISO image           |
| `qemu-system-x86_64` | Emulator for testing         |

### Top-level Targets

| Target          | Description                                                  |
|-----------------|--------------------------------------------------------------|
| `make`          | Kernel ELF + all modules                                     |
| `make iso`      | Bootable GRUB ISO at `build/x86_64/kernel-x86_64.iso`        |
| `make run`      | ISO in QEMU (128 MB RAM, serial stdio + COM2 tcp:4444)       |
| `make DEBUG=1`  | Includes `exception_test` module + `-D__DEBUG__` flag        |
| `make clean`    | Remove all build artifacts                                   |
| `make VERBOSE=1`| Show full compilation/link commands                          |

### Common CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -O2
```

SSE/MMX disabled because the kernel does not save FPU state on context switches.
`-mno-red-zone` required by interrupt handlers.

---

## Module Binary Format (flat binary)

Kernel modules (hello, exception_test, command_kern) are **flat binaries** loaded by GRUB as `module2`. Layout at offset 0:

```
[module_header_t — 56 bytes]  magic + name[32] + version[16] + reserved
[entry function]               immediately after, at offset 56
[rest of code/data]            .text, .rodata, .data, .bss, .got
```

The kernel calls `entry = module_start + 56` with signature:
```c
void entry(const kst_t* kst);
```

---

## Program Format (ELF64)

Programs loaded at runtime by COMMAND.KERN are **standard ELF64 executables** (`ET_EXEC`). They are compiled with:
- Cross-compiler `x86_64-elf-gcc`
- CRT: `newlib/arch/x86_64/crt.S` → entry `_prog_entry`
- Libc: `libnewlib.a` + newlib headers
- Linker script: `newlib/arch/x86_64/prog.ld` (base address 32 MB)

The kernel ELF loader (`kernel/elf.c`) reads `PT_LOAD` segments and copies them to their `p_vaddr` in the identity-mapped memory space.

---

## Kernel Services Table (KST v2)

```c
typedef struct {
    uint32_t version;    // KST_VERSION = 2
    struct { print, print_hex, print_dec, putchar, clear, getchar } console;
    struct { sbrk }                                                  mem;
    struct { write, read, open, close, isatty, lseek, fstat }       io;
    struct { exit, panic, getpid, set_exc_hook, clear_exc_hook,
             elf_load }                                              sys;
} kst_t;
```

`kst->sys.elf_load(data, size)` — loads an ELF64 and returns its entry point.

---

## Boot Sequence

```
GRUB (Multiboot2)
  └─ _start [boot.S, 32-bit]
       ├─ validate Multiboot2 magic
       ├─ zero BSS
       ├─ setup page tables: identity map 0–128 MB (64 × 2MB huge pages)
       ├─ enable PAE + Long Mode (EFER) + Paging
       ├─ load 64-bit GDT, far jump → long_mode_entry
       └─ call kernel_main(boot_magic, boot_addr)

kernel_main [kernel.c]
  ├─ hal_console_init()     — VGA or Serial depending on cmdline
  ├─ hal_idt_init()         — IDT + TSS (IST1 for #DF)
  ├─ hal_arch_init()        — parse MB2: memory map + modules
  ├─ hal_mem_init()         — initialize PMM (bitmap allocator, up to 512 MB)
  ├─ heap_init(kernel_end)  — 4 MB bump-allocator heap on PMM
  ├─ modules_run_all()      — run auxiliary modules (non-entry)
  └─ modules_launch_entry() — launch COMMAND.KERN → interactive loop
```
