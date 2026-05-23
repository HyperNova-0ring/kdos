# KDOS — Project Overview

KDOS is a bare-metal x86_64 hobby operating system written in C and GAS assembly. It boots via GRUB/Multiboot2, transitions from 32-bit protected mode to 64-bit long mode, and runs a module-based architecture where all code executes at ring 0.

## Repository Structure

```
kdos/
├── Makefile              — Top-level build orchestrator
├── kernel/               — Core kernel
├── newlib/               — C runtime and libc glue for modules
├── command_kern/         — Primary shell module (analogous to COMMAND.COM)
├── build/                — Output artifacts (generated)
└── doc/                  — This documentation
```

## Design Philosophy

- **No syscalls.** Communication between the kernel and modules uses the Kernel Services Table (KST), a struct of function pointers passed by value to each module's entry point.
- **Ring 0 only.** All code runs at privilege level 0. There is no user space yet.
- **Flat binary modules.** Modules are raw binaries loaded by GRUB as `module2` entries. The kernel discovers them from the Multiboot2 info structure.
- **Identity-mapped memory.** Physical address == virtual address for the entire loaded range (0–128 MB). No virtual memory manager yet.

## Subprojects

| Subproject     | Role                                                          |
|----------------|---------------------------------------------------------------|
| `kernel/`      | Boot, HAL, PMM, heap, IDT, module loader, KST                |
| `newlib/`      | CRT startup (`crt.S`), libc syscall stubs (`newlib.c`)       |
| `command_kern/`| Entry-point shell module launched after all other modules run |

## Build System

The root Makefile builds all three subprojects and assembles the final ISO.

### Toolchain

| Tool              | Role                  |
|-------------------|-----------------------|
| `x86_64-elf-gcc`  | C compiler (freestanding) |
| `x86_64-elf-ld`   | Linker                |
| `x86_64-elf-objcopy` | ELF → raw binary   |
| `x86_64-elf-ar`   | Static library archiver |
| `grub-mkrescue`   | ISO image creation    |
| `qemu-system-x86_64` | Emulator for testing |

### Common CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -O2
```

SSE/MMX disabled because the kernel does not save/restore FPU state on context switches.  
`-mno-red-zone` required in kernel code because interrupt handlers can fire at any time.

### Top-level Targets

| Target  | Description                                                  |
|---------|--------------------------------------------------------------|
| `make`  | Build kernel ELF + all modules                              |
| `make iso` | Create bootable ISO (`build/x86_64/kernel-x86_64.iso`) |
| `make run` | Build ISO and launch in QEMU (128 MB RAM, serial stdio) |
| `make clean` | Remove all build artifacts                           |
| `make VERBOSE=1` | Show full compiler/linker commands               |
| `make DEBUG=1` | Include debug modules and add `-D__DEBUG__`         |

### Build Output Layout

```
build/x86_64/
├── kernel.elf            — Kernel ELF
├── kernel-x86_64.iso     — Bootable GRUB ISO
├── crt.o                 — CRT startup object (from newlib/)
├── libnewlib.a           — Libc glue static library (from newlib/)
├── module.ld             — Module linker script (from newlib/)
├── modules/
│   ├── hello.bin         — Example module
│   └── command.kern.bin  — Main shell module
└── iso/
    ├── boot/
    │   ├── kernel.elf
    │   └── grub/grub.cfg
    └── modules/
        ├── hello.bin
        └── command.kern.bin
```

## Module Binary Format

Every module binary has this layout at offset 0:

```
[module_header_t]     — 56 bytes: magic, name[32], version[16], reserved
[entry function]      — starts immediately after the header
[rest of code/data]   — .text, .rodata, .data, .bss, .got
```

The kernel computes `entry = module_start + sizeof(module_header_t)` and calls it as:

```c
typedef void (*module_entry_fn)(const kst_t* kst);
```

## Kernel Services Table (KST)

The KST (`kst_t`, defined in `kernel/module_abi.h`) is the contract between the kernel and modules. It replaces syscalls.

```c
typedef struct {
    uint32_t version;       // KST_VERSION = 1
    struct { ... } console; // print, putchar, print_hex, print_dec, clear, getchar
    struct { ... } mem;     // sbrk
    struct { ... } io;      // write, read, open, close, isatty, lseek, fstat
    struct { ... } sys;     // exit, panic, getpid
} kst_t;
```

Currently implemented (non-stub):
- `console.print/putchar/print_hex/print_dec/clear` → VGA text mode
- `io.write` (fd 1, 2) → VGA console
- `io.isatty` (fd 0–2) → returns 1
- `mem.sbrk` → kernel bump-allocator heap
- `sys.panic` → kernel panic with red screen + halt
- `sys.exit` → triggers kernel panic (no process model yet)

Stubs (return -1 or safe default):
- `console.getchar`, `io.read`, `io.open`, `io.close`, `io.lseek`, `io.fstat`, `sys.getpid`

## Boot Sequence

```
GRUB (Multiboot2)
  └─ _start (32-bit, boot.S)
       ├─ validate Multiboot2 magic
       ├─ zero BSS
       ├─ setup PML4/PDPT/PD page tables (identity map 0–128 MB)
       ├─ enable PAE, Long Mode (EFER), Paging
       ├─ load 64-bit GDT
       ├─ far jump → long_mode_entry (64-bit)
       └─ call kernel_main(boot_magic, boot_addr)

kernel_main (kernel.c)
  ├─ hal_console_init()     — VGA init
  ├─ hal_console_clear()    — clear screen
  ├─ hal_idt_init()         — install 32 exception handlers
  ├─ print banner
  ├─ hal_arch_init()        — parse Multiboot2: memory map + module registration
  ├─ hal_mem_init()         — PMM init from memory map
  ├─ heap_init(kernel_end)  — 4 MB bump-allocator heap
  ├─ print memory map
  ├─ modules_run_all()      — run non-entry modules
  └─ modules_launch_entry() — launch COMMAND.KERN or command.com → halt on return
```
