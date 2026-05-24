# KDOS command_kern — Documentation

`command_kern/` is the primary entry-point module of KDOS, analogous to `COMMAND.COM` in MS-DOS. It is the first interactive layer of the OS: it provides a command interpreter that lets you load and execute ELF programs transferred from the host.

## Directory Structure

```
command_kern/
├── Makefile
├── main.c        — Interactive shell: readline, dispatch, commands
├── module.ld     — Flat binary module linker script
├── transfer.h    — transfer_recv_elf() declaration
├── transfer.c    — ELF reception over COM2 (KELF protocol)
├── recover.h     — exc_save() / exc_restore declaration
└── recover.S     — Context save/restore for exception recovery
```

---

## Current Functionality

COMMAND.KERN implements a full interactive command interpreter:

| Command | Description |
|---------|-------------|
| `help`  | Show available commands |
| `clear` | Clear the screen |
| `halt`  | Halt the system (calls `kst->sys.panic`) |
| `load`  | Receive an ELF over COM2 (KELF protocol) and run it |

The main loop:
1. Prints the `> ` prompt.
2. Reads a line with `readline` (echo, backspace, max 128 chars).
3. Dispatches to the matching command function.

---

## Build

```bash
# From repo root:
make              # builds command.kern.bin as part of all

# From command_kern/ directly:
make -C command_kern ARCH=x86_64
```

### Output

| Artifact                           | Description                  |
|------------------------------------|------------------------------|
| `build/x86_64/command.kern.elf`    | ELF intermediate             |
| `build/x86_64/command.kern.bin`    | Final flat binary            |

### Compiler Flags

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -fPIC -O2
-falign-functions=1 -falign-loops=1 -falign-jumps=1
-I../kernel
```

`-fPIC` required because the module loads at an arbitrary physical address.

---

## ELF Transfer Protocol — KELF

The `load` command waits for an ELF on COM2 packaged in the KELF protocol:

```
Bytes 0–3 : magic = { 'K', 'E', 'L', 'F' }
Bytes 4–7 : size  = uint32_t, total ELF byte count (little-endian)
Bytes 8+  : raw ELF data
```

COM2 (0x2F8) is initialized to 115200 baud, 8N1, FIFO enabled, on the first call to `transfer_recv_elf`. Hard cap: 2 MB.

### Sending from the host (Python)

```python
python3 -c "
    import sys, struct
    d = open('prog.elf','rb').read()
    sys.stdout.buffer.write(b'KELF' + struct.pack('<I', len(d)) + d)
" | nc localhost 4444
```

(QEMU exposes COM2 on `tcp:4444` with `make run`.)

---

## ELF Load and Execution

When `transfer_recv_elf` succeeds:

1. Prints the received byte count.
2. Calls `kst->sys.elf_load(buf, size)` — the neutral kernel ELF loader.
3. If `elf_load` returns 0: prints an error and returns to the prompt.
4. On success: prints the entry point address and calls `elf_exec`.

`elf_exec` installs an exception hook, calls the program, and on return:
- No exception: prints `"Program finished."`.
- Exception caught: prints the exception name, RIP, and error code.

---

## Exception Recovery — `recover.S` + `exc_hook_fn`

COMMAND.KERN catches exceptions raised by ELF programs without crashing the shell. The mechanism:

### `exc_save()` / `exc_restore`

```c
int exc_save(void);       // saves callee-saved registers + RSP + return address
extern void exc_restore;  // IRETQ target: restores saved state and returns 1
```

`exc_save()` returns 0 on first call. If the program faults, the IDT calls the hook, which makes IRETQ jump to `exc_restore`, which restores the `elf_exec` frame and returns 1.

### `exc_hook_fn`

```c
static void exc_hook_fn(uint64_t vec, uint64_t rip, uint64_t err,
                         uint64_t* out_rip, uint64_t* out_rsp) {
    exc_vec = vec; exc_rip = rip; exc_err = err;
    __asm__ volatile ("leaq exc_restore(%%rip), %0" : "=r"(*out_rip));
    *out_rsp = exc_restore_rsp;
}
```

`leaq exc_restore(%rip)` resolves the address at runtime (PCREL), correct for a PIC module without a dynamic linker.

---

## BSS Zeroing — PIC Flat Binary Module

GRUB does not zero flat binary module memory. `module_main` does it explicitly at startup using RIP-relative inline asm (C with `-fPIC` cannot be used here since the GOT is not relocated):

```c
__asm__ volatile (
    "leaq __bss_start(%%rip), %%rdi\n\t"
    "leaq __bss_end(%%rip),   %%rax\n\t"
    "subq %%rdi, %%rax\n\t"
    "jz   1f\n\t"
    "movq %%rax, %%rcx\n\t"
    "xorl %%eax, %%eax\n\t"
    "rep  stosb\n\t"
    "1:"
    : : : "rax", "rcx", "rdi", "memory"
);
```

`__bss_start` and `__bss_end` are exported by `module.ld`.

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
.bss    ALIGN(1) : {
    __bss_start = .;
    *(.bss .bss.*)
    __bss_end = .;
}
/DISCARD/ : { .eh_frame .comment .note.* }
```

Base address 0; the kernel reads the header at offset 0 and calls the entry at offset 56 (`sizeof(module_header_t)`).

---

## How the Kernel Launches It

`modules_launch_entry()` in `kernel/modules.c` searches for a module named `COMMAND.KERN` (checked against both cmdline and `module_header_t.name`). When found:

1. Prints `"Launching COMMAND.KERN..."`.
2. Computes entry = `module_start + sizeof(module_header_t)` (= `module_start + 56`).
3. Calls `entry(&kernel_kst)`.
4. COMMAND.KERN runs in an infinite loop (`for (;;) { readline + dispatch }`). If it returns, `kernel_main` calls `hal_panic`.

---

## ABI Used

`command_kern` uses `module_abi.h` directly, without CRT or newlib. It follows the bare module ABI:

```c
typedef void (*module_entry_fn)(const kst_t* kst);
```

The KST provides everything needed: console, heap (for the ELF receive buffer), `elf_load`, and the exception hook.

---

## GRUB Configuration

The root Makefile auto-generates `grub.cfg` during `make iso`. For each `.bin` in `build/x86_64/iso/modules/`:

```
module2 /modules/command.kern.bin command.kern.bin
```

GRUB passes the filename as the module command line. The kernel stores this in `module_t.cmdline` and also identifies the module by `module_header_t.name = "COMMAND.KERN"`.
