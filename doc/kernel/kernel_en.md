# KDOS Kernel Documentation

The kernel is the core of KDOS. It handles boot, hardware abstraction, physical memory management, interrupt handling, the heap, and the module loading/dispatch system.

## Directory Structure

```
kernel/
├── kernel.c                    — Entry point: kernel_main()
├── hal.h                       — Hardware Abstraction Layer public interface
├── module_abi.h                — KST and module binary contract (shared with modules)
├── modules.h / modules.c       — Module registry and dispatcher
├── heap.h / heap.c             — Kernel heap (bump allocator)
├── Makefile
├── modules/
│   └── hello/
│       ├── main.c              — Minimal example module
│       └── module.ld           — Module linker script
└── arch/
    └── x86_64/
        ├── boot.S              — Multiboot2 header, 32→64 transition, GDT, paging
        ├── hal.c               — HAL implementation for x86_64
        ├── vga.h / vga.c       — BIOS VGA text-mode driver (80×25)
        ├── idt.h / idt.c       — Interrupt Descriptor Table (IDT) setup
        ├── isr.S               — 32 CPU exception ISR stubs
        ├── pmm.h / pmm.c       — Physical Memory Manager (bitmap allocator)
        ├── multiboot2.h        — Multiboot2 tag structures
        ├── link.ld             — Kernel linker script
        └── modules/debug/
            └── exception_test/ — Debug module: fires CPU exceptions (DEBUG=1 only)
```

## Build

```bash
# From repo root:
make              # builds kernel ELF + all modules
make DEBUG=1      # includes exception_test module + -D__DEBUG__

# From kernel/ directly:
make -C kernel ARCH=x86_64
```

Output: `kernel/build/kernel-x86_64.elf` + `kernel/build/modules/*.bin`

### CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -I<arch_dir> -I. -O2
```

Module-specific extra flags: `-falign-functions=1 -falign-loops=1 -falign-jumps=1 -fPIC`

---

## Boot Sequence — `arch/x86_64/boot.S`

GRUB loads the kernel ELF in 32-bit protected mode and jumps to `_start`.

### 32-bit setup

1. **Stack**: 16 KB static stack in BSS.
2. **Multiboot2 validation**: checks `eax == 0x36D76289`; hangs on mismatch.
3. **BSS zero**: `rep stosl` from `bss_start` to `bss_end`.
4. **Page tables** (`setup_page_tables`):
   - PML4[0] → PDPT (present + writable)
   - PDPT[0] → PD (present + writable)
   - PD[0..63] → 64 × 2 MB huge pages with PS bit (identity maps 0–128 MB)
5. **PAE**: set bit 5 of CR4.
6. **CR3**: load physical address of PML4.
7. **Long Mode**: set bit 8 in EFER MSR (0xC0000080).
8. **Paging**: set bit 31 in CR0.
9. **GDT**: load 64-bit GDT (null, code 0x08, data 0x10).
10. **Far jump**: `ljmp $0x08, $long_mode_entry` — switches to 64-bit code segment.

### 64-bit entry (`long_mode_entry`)

Sets all data segment registers to 0x10, loads boot arguments into `rdi`/`rsi`, and calls `kernel_main(boot_magic, boot_addr)`. On return: `cli` + `hlt` loop.

---

## Hardware Abstraction Layer — `hal.h` / `arch/x86_64/hal.c`

`hal.h` defines the architecture-neutral interface. `hal.c` implements it for x86_64. The kernel core (`kernel.c`, `modules.c`, `heap.c`) only includes `hal.h` — it never references arch-specific files directly.

### Console API

| Function                           | Description                     |
|------------------------------------|---------------------------------|
| `hal_console_init()`               | Initialize VGA text mode        |
| `hal_console_putchar(char c)`      | Write one character             |
| `hal_console_print(const char*)`   | Write a string                  |
| `hal_console_print_hex(uintptr_t)` | Print 64-bit value as 0xXXX...  |
| `hal_console_print_dec(size_t)`    | Print decimal number            |
| `hal_console_clear()`              | Clear screen                    |

### Memory API

```c
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;   // 1 = usable, 2+ = reserved
} hal_mem_region_t;

void     hal_mem_set_map(hal_mem_region_t* map, uint32_t count);
uint32_t hal_mem_get_map(hal_mem_region_t* out, uint32_t max);
void     hal_mem_init(void);
uintptr_t hal_mem_alloc_pages(uint32_t n);   // returns phys addr, 0 = OOM
void      hal_mem_free_pages(uintptr_t addr, uint32_t n);
```

### CPU API

```c
void hal_cpu_halt(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_enable_interrupts(void);
```

### Other

```c
void hal_arch_init(uint64_t boot_magic, uint64_t boot_addr);  // parse Multiboot2
void hal_idt_init(void);
void hal_panic(const char* msg);  // disable interrupts, red screen, halt
```

---

## VGA Driver — `arch/x86_64/vga.c`

BIOS VGA text mode: 80 columns × 25 rows at physical address `0xB8000`. Each cell is a 16-bit value: low byte = ASCII character, high byte = color (4-bit bg | 4-bit fg).

### Colors

16 colors defined in `vga_color` enum: `VGA_BLACK` (0) through `VGA_WHITE` (15).

### API

| Function                              | Description                               |
|---------------------------------------|-------------------------------------------|
| `vga_init()`                          | Initialize, set white-on-black, clear     |
| `vga_clear()`                         | Fill all cells with spaces, reset cursor  |
| `vga_putchar(char c)`                 | Write char; handles `\n`, `\r`, `\t`, wrap, scroll |
| `vga_print(const char* str)`          | Write string                              |
| `vga_print_hex(uint64_t)`             | Print `0xXXXXXXXXXXXXXXXX`               |
| `vga_set_color(vga_color fg, bg)`     | Set foreground + background color         |

Scrolling: when `cursor_row >= 25`, copies row N to row N-1 for all rows, clears row 24.

---

## Physical Memory Manager — `arch/x86_64/pmm.c`

Bitmap-based physical page frame allocator.

### Constants

- `PAGE_SIZE = 4096`
- `MAX_FRAMES = 512 MB / 4 KB = 131072` (16 KB bitmap in BSS)

### Initialization (`pmm_init`)

1. Start: mark all frames as used (`0xFF`).
2. Determine `total_frames` from the highest address in a usable Multiboot2 memory region.
3. Mark every page-aligned address inside usable regions as free.
4. Re-mark `[0, kernel_end)` as used — protects BIOS area, kernel code/data/BSS.

### Allocation (`pmm_alloc_pages`)

First-fit search: finds `n` contiguous free frames, marks them used, returns their base physical address. Returns 0 on OOM.

### Deallocation (`pmm_free_pages`)

Marks `n` frames starting at `addr` as free.

---

## Kernel Heap — `heap.c`

A bump (linear) allocator backed by the PMM, implementing the `sbrk` interface.

### Constants

- `HEAP_PAGES = 1024` → **4 MB** heap

### Initialization (`heap_init`)

Rounds `base` (= `&kernel_end`) up to a page boundary, then calls `hal_mem_alloc_pages(1024)`. Panics if the physical address returned does not equal `heap_base` (would indicate identity mapping is broken).

### `heap_sbrk(intptr_t incr)`

- Advances the bump pointer by `incr` bytes.
- Returns the old pointer (POSIX sbrk semantics).
- Returns `(void*)-1` if `incr <= 0` or would exceed `heap_limit`.

This is wired into `kst->mem.sbrk`, so `malloc` in newlib-linked modules works automatically.

---

## IDT and Exceptions — `arch/x86_64/idt.c` + `isr.S`

### IDT Structure

256 entries, each an 8-byte `idt_entry_t` with a split 64-bit handler offset, selector, and type attributes. The IDT pointer is loaded via `lidt`.

Only vectors 0–31 (CPU exceptions) are installed. All use `IDT_INTERRUPT_GATE` (0x8E: P=1, DPL=0, type=1110), meaning interrupts are automatically disabled on entry.

### ISR Stubs (`isr.S`)

Two macros keep stubs uniform:

- `ISR_NOERR num` — pushes dummy error code `0`, then the vector number, jumps to `isr_common_stub`.
- `ISR_ERR num` — the CPU already pushed an error code; just pushes the vector number, jumps to `isr_common_stub`.

Vectors with error codes: 8 (#DF), 10 (#TS), 11 (#NP), 12 (#SS), 13 (#GP), 14 (#PF), 17 (#AC), 30 (#SX).

### Common Stub

`isr_common_stub` saves all 15 general-purpose registers (`rax`–`r15`), passes `rsp` (which now points to a complete `interrupt_frame_t`) as the first argument, and calls `exception_handler`.

### `interrupt_frame_t`

```c
typedef struct {
    uint64_t r15..r8, rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    // pushed by CPU:
    uint64_t rip, cs, rflags, rsp, ss;
} interrupt_frame_t;
```

### Exception Handler

Displays exception name (from a 32-entry string table), RIP, CS, RSP, SS, RFLAGS, error code (if nonzero), RAX, RBX, RCX, RDX — all on a red background. Then halts.

---

## Module System — `modules.h` / `modules.c`

### Data Structures

```c
typedef struct {
    uintptr_t      start;       // physical start address
    uintptr_t      end;         // physical end address
    char           cmdline[64]; // GRUB module command line
    module_state_t state;       // ABSENT / LOADED / RUNNING / FAILED
    module_header_t* header;    // pointer to header at start, or NULL
} module_t;

typedef struct {
    module_t modules[MAX_MODULES]; // MAX_MODULES = 16
    uint32_t count;
} module_list_t;
```

### Registration (`modules_register`)

Called from `hal_arch_init` for each `MB2_TAG_MODULE` found in the Multiboot2 info. Validates the `MODULE_MAGIC` at `start`; on mismatch sets state to `FAILED` and prints a warning.

### `modules_run_all()`

Iterates all modules with state `LOADED` and a valid header, skipping entry shells (`COMMAND.KERN`, `command.com`). For each: prints name + version, then calls `entry(&kernel_kst)`.

### `modules_launch_entry()`

Tries to find `COMMAND.KERN` first, then `command.com`. Launches the first match. If neither is found, prints an error message and returns (kernel then panics).

### Kernel Services Table — `kernel_kst`

A static `kst_t` defined in `modules.c`. Populated at compile time with HAL function pointers and stub implementations.

---

## Module ABI — `module_abi.h`

Shared by the kernel and all modules.

### `module_header_t`

```c
typedef struct {
    uint32_t magic;      // MODULE_MAGIC = 0x444F534D ("DOSM")
    char     name[32];
    char     version[16];
    uint32_t reserved;
} __attribute__((packed)) module_header_t;
```

Total size: 56 bytes. Placed at offset 0 of every module binary via the `.module_header` section.

### `MODULE_HEADER(name, version)` Macro

Embeds the header at file scope:

```c
MODULE_HEADER("hello", "0.1");
```

Uses `__attribute__((section(".module_header"), used))` to prevent dead-code elimination.

### `module_entry_fn`

```c
typedef void (*module_entry_fn)(const kst_t* kst);
```

The entry function always resides at `module_start + sizeof(module_header_t)`.

---

## Example Module — `modules/hello/`

```c
#include "../../module_abi.h"

MODULE_HEADER("hello", "0.1");

__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    kst->console.print("Hello World!\n");
}
```

Uses `module_abi.h` directly (no CRT or newlib). The `__attribute__((section(".text.entry")))` ensures the function is the first in `.text` after the header — directly callable at `start + 56`.

### Module Linker Script (`modules/hello/module.ld`)

```
. = 0;
.module_header : { KEEP(*(.module_header)) }
.text ALIGN(1) : { *(.text.entry) *(.text .text.*) }
.rodata, .data, .got, .got.plt, .bss (all ALIGN(1))
/DISCARD/: .eh_frame, .comment
```

---

## Debug Module — `arch/x86_64/modules/debug/exception_test/`

Only included when `DEBUG=1`. Tests the IDT by deliberately triggering four exception types:

| ID | Exception | Method                    |
|----|-----------|---------------------------|
| 0  | #DE       | Integer divide by zero    |
| 1  | #UD       | `ud2` instruction         |
| 2  | #PF       | Null pointer dereference  |
| 3  | #GP       | `rdmsr` with invalid index |

The active test is selected at compile time via `#define TEST_EXCEPTION N`.

---

## Linker Script — `arch/x86_64/link.ld`

```
ENTRY(_start)

. = 1M    (load address: 1 MB physical)

.multiboot  ALIGN(8)  — Multiboot2 header (GRUB searches first 32 KB)
.text       ALIGN(4K) — Code
.rodata     ALIGN(4K)
.data       ALIGN(4K)
.bss        ALIGN(4K) — bss_start … bss_end symbols exported to boot.S

kernel_end = ALIGN(4K)  — exported; used by heap.c and pmm.c
```

The kernel loads at the 1 MB mark, which is the traditional safe load address above the BIOS data area.
