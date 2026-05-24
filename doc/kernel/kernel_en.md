# KDOS Kernel Documentation

The kernel is the core of KDOS. It handles boot, hardware abstraction, physical memory, interrupt handling, the heap, the neutral ELF loader, and the module system.

## Directory Structure

```
kernel/
├── kernel.c                    — Entry point: kernel_main()
├── hal.h                       — HAL public interface (architecture-neutral)
├── module_abi.h                — KST and module ABI contract (shared with modules)
├── modules.h / modules.c       — Module registry, dispatcher, and static KST
├── heap.h / heap.c             — Kernel heap (bump allocator)
├── elf.h / elf.c               — Neutral, portable ELF64 loader
├── Makefile
├── modules/
│   └── hello/                  — Minimal example module
└── arch/
    └── x86_64/
        ├── boot.S              — Multiboot2 header, 32→64 transition, GDT, paging
        ├── hal.c               — HAL implementation for x86_64
        ├── vga.h / vga.c       — VGA text-mode driver (80×25)
        ├── serial.h / serial.c — Serial driver (COM1) for alternate console
        ├── idt.h / idt.c       — IDT + TSS64 (IST1 for #DF)
        ├── isr.S               — 32 CPU exception ISR stubs
        ├── pmm.h / pmm.c       — Physical Memory Manager (bitmap allocator)
        ├── elf_arch.h          — x86_64-specific ELF constants
        ├── multiboot2.h        — Multiboot2 tag structures
        ├── link.ld             — Kernel linker script
        └── modules/debug/
            └── exception_test/ — Debug module (DEBUG=1 only)
```

---

## Two Kernel Layers

The kernel code is split into two strict layers:

| Layer | Files | Rule |
|-------|-------|------|
| **Neutral** | `kernel/*.c`, `kernel/*.h` | Only includes `hal.h`. Portable across architectures. |
| **Arch-specific** | `kernel/arch/{arch}/` | Implements the HAL and provides `elf_arch.h`. |

`kernel/elf.c` includes `elf_arch.h` via `-I$(ARCH_DIR)` in the Makefile. This keeps the loader neutral without hardcoding architecture constants.

---

## Build

```bash
# From repo root:
make              # kernel ELF + all modules
make DEBUG=1      # includes exception_test + -D__DEBUG__

# From kernel/ directly:
make -C kernel ARCH=x86_64
```

### CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -I<arch_dir> -I. -O2
```

Extra flags for modules: `-falign-functions=1 -falign-loops=1 -falign-jumps=1 -fPIC`

---

## Boot Sequence — `arch/x86_64/boot.S`

GRUB loads the kernel ELF in 32-bit protected mode and jumps to `_start`.

### 32-bit setup

1. **Stack**: 16 KB static stack in BSS.
2. **Multiboot2 validation**: checks `eax == 0x36D76289`; `hlt` loop on mismatch.
3. **BSS zero**: `rep stosl` from `bss_start` to `bss_end`.
4. **Page tables** (`setup_page_tables`):
   - PML4[0] → PDPT → PD with 64 × 2 MB huge pages (identity maps 0–128 MB).
5. **PAE** (CR4 bit 5), **CR3** (PML4), **Long Mode** (EFER bit 8), **Paging** (CR0 bit 31).
6. **64-bit GDT**: null + code (0x08) + data (0x10).
7. **Far jump**: `ljmp $0x08, $long_mode_entry`.

### 64-bit entry (`long_mode_entry`)

Sets data segment registers to 0x10, loads boot arguments into `rdi`/`rsi`, calls `kernel_main`. On return: `cli` + `hlt` loop.

---

## HAL — `hal.h` / `arch/x86_64/hal.c`

`hal.h` defines the architecture-neutral interface. The portable kernel core only includes `hal.h`.

### Console API

| Function | Description |
|----------|-------------|
| `hal_console_init(type)` | Initialize VGA or Serial based on `type` |
| `hal_console_putchar(c)` | Write one character |
| `hal_console_print(str)` | Write a string |
| `hal_console_print_hex(uint64_t)` | Print `0xXXXXXXXXXXXXXXXX` |
| `hal_console_print_dec(uint64_t)` | Print decimal number |
| `hal_console_clear()` | Clear screen |
| `hal_console_getchar()` | Read a character (-1 if no input) |
| `hal_console_panic_color()` | Set panic color on VGA (red); no-op on serial |

### Memory API

```c
void      hal_mem_set_map(hal_mem_region_t* map, uint32_t count);
uint32_t  hal_mem_get_map(hal_mem_region_t* out, uint32_t max);
void      hal_mem_init(void);
uintptr_t hal_mem_alloc_pages(uint32_t n);   // 0 = OOM
void      hal_mem_free_pages(uintptr_t addr, uint32_t n);
```

### CPU and Interrupt API

```c
void hal_cpu_halt(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_enable_interrupts(void);
void hal_arch_init(uint64_t boot_magic, uint64_t boot_addr);
void hal_idt_init(void);
void hal_panic(const char* msg);

// Program-level exception hook (see command_kern)
void hal_set_exc_hook(void (*fn)(uint64_t vec, uint64_t rip, uint64_t err,
                                  uint64_t* out_rip, uint64_t* out_rsp));
void hal_clear_exc_hook(void);
```

---

## VGA Driver — `arch/x86_64/vga.c`

BIOS VGA text mode: 80 × 25 columns at `0xB8000`. Each cell: ASCII byte (low) + color byte (high). Supports `\n`, `\r`, `\t`, line wrap, scroll.

---

## Serial Driver — `arch/x86_64/serial.c`

COM1 (0x3F8) at 115200 baud, 8N1. The serial console activates when the GRUB cmdline contains `console=serial`. The active console (VGA or serial) is selected in `hal_console_init` and applies to all `hal_console_*` functions.

---

## Physical Memory Manager — `arch/x86_64/pmm.c`

Bitmap-based physical frame allocator (1 bit / 4 KB page).

- `MAX_FRAMES = 131072` (512 MB / 4 KB, 16 KB bitmap in BSS).
- `pmm_init`: marks all frames used, frees usable Multiboot2 regions, re-marks `[0, kernel_end)` as used.
- `pmm_alloc_pages(n)`: first-fit, returns base physical address or 0.
- `pmm_free_pages(addr, n)`: bounds-checked (`i >= MAX_FRAMES` → stop).

---

## Kernel Heap — `heap.c`

Bump allocator backed by the PMM. `HEAP_PAGES = 1024` → **4 MB**.

### `heap_sbrk(intptr_t incr)`

- `incr == 0` → returns current break (newlib `malloc` probes this way).
- `incr > 0` → advances bump pointer, returns old pointer.
- `incr < 0` or overflow → returns `(void*)-1`.

Wired into `kst->mem.sbrk`, so `malloc` in newlib-linked programs works automatically.

---

## ELF Loader — `elf.c` / `elf.h` / `arch/x86_64/elf_arch.h`

The ELF loader is part of the neutral kernel layer. It is portable: architecture-dependent constants come from `elf_arch.h`, included automatically via `-I$(ARCH_DIR)`.

### `arch/x86_64/elf_arch.h`

```c
#define ELF_CLASS    2     // ELFCLASS64
#define ELF_MACHINE  62    // EM_X86_64
#define PROG_LOAD_MIN  0x100000UL    // 1 MB — above kernel
#define PROG_LOAD_MAX  0x8000000UL   // 128 MB — identity map limit
```

### `uintptr_t elf_load(const void* data, size_t size)`

Validates: ELF magic, `ELF_CLASS`, `ET_EXEC`, `ELF_MACHINE`, `e_phoff`, `e_phentsize`.  
For each `PT_LOAD` segment: overflow-safe bounds checks (`p_memsz > LOAD_VADDR_MAX - p_vaddr`), copies `p_filesz` bytes to `p_vaddr`, zeroes BSS padding.  
Returns `e_entry` if at least one segment was loaded, 0 on error.

Exposed in the KST as `kst->sys.elf_load`.

---

## IDT and Exceptions — `arch/x86_64/idt.c` + `isr.S`

### IDT Structure

256 `idt_entry_t` entries (split 64-bit offset, selector, type attributes). Only vectors 0–31 (CPU exceptions) are installed, all as `IDT_INTERRUPT_GATE` (0x8E: P=1, DPL=0, type=1110).

### TSS64 and IST1 (for #DF)

`idt_init` sets up a static `tss64_t` with a dedicated 4 KB IST1 stack. Extends the GDT to 5 entries (null/code/data/TSS_low/TSS_high), reloads with `lgdt`, loads the Task Register with `ltr 0x18`. IDT vector 8 (#DF) descriptor has `ist = 1` → uses the IST1 stack even if the kernel stack has overflowed.

### Exception Hook

The exception handler checks for a registered user hook (via `hal_set_exc_hook`). If one exists: fills `*out_rip`/`*out_rsp` with the recovery address and IRETQ redirects there. The hook is auto-cleared on entry. Used by `command_kern` to catch ELF program exceptions without crashing the shell.

### Exception Handler

Calls `hal_console_panic_color()` (red on VGA, no-op on serial) and `hal_console_*` to display the exception name, RIP, CS, RSP, RFLAGS, error code, and registers. If there is a hook: calls it and IRETs to the recovery address. If no hook: halts.

---

## Module System — `modules.c`

### `modules_register(start, end, cmdline)`

Called from `hal_arch_init` for each `MB2_TAG_MODULE`. Validates `MODULE_MAGIC` at `start`; on mismatch sets state to `FAILED`.

### `modules_run_all()`

Runs all modules with state `LOADED` and a valid header, excluding entry shells (`COMMAND.KERN`, `command.com`).

### `modules_launch_entry()`

Finds `COMMAND.KERN` or `command.com` and hands execution over by calling `entry(&kernel_kst)`.

### Static KST — `kernel_kst`

Static `kst_t` defined in `modules.c`. Populated at compile time with HAL function pointers and syscall implementations. The address of `kernel_kst` never changes: `kst->sys.elf_load` is as simple as a function pointer.

---

## Module ABI — `module_abi.h`

Shared by the kernel and all modules.

### KST v2

```c
#define KST_VERSION 2

typedef struct {
    uint32_t version;
    struct { print, print_hex, print_dec, putchar, clear, getchar } console;
    struct { sbrk }                                                  mem;
    struct { write, read, open, close, isatty, lseek, fstat }       io;
    struct { exit, panic, getpid,
             set_exc_hook, clear_exc_hook,
             elf_load }                                              sys;
} kst_t;
```

`elf_load` was added in v2. It lets any module load ELF programs without implementing its own loader.

### `module_header_t` (56 bytes)

```c
typedef struct {
    uint32_t magic;      // MODULE_MAGIC = 0x444F534D ("DOSM")
    char     name[32];
    char     version[16];
    uint32_t reserved;
} __attribute__((packed)) module_header_t;
```

### `MODULE_HEADER(name, version)` Macro

Embeds the header at offset 0 of the flat binary via the `.module_header` section.

---

## Kernel Linker Script — `arch/x86_64/link.ld`

```
ENTRY(_start)
. = 1M           — load address: 1 MB physical (above BIOS data area)

.multiboot  ALIGN(8)   — Multiboot2 header
.text       ALIGN(4K)
.rodata     ALIGN(4K)
.data       ALIGN(4K)
.bss        ALIGN(4K)  — bss_start … bss_end exported to boot.S

kernel_end = ALIGN(4K) — exported to heap.c and pmm.c
```
