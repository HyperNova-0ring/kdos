# Documentación del Kernel KDOS

El kernel es el núcleo de KDOS. Gestiona el arranque, la abstracción de hardware, la memoria física, las interrupciones, el heap, el cargador ELF neutral y el sistema de módulos.

## Estructura de Directorios

```
kernel/
├── kernel.c                    — Punto de entrada: kernel_main()
├── hal.h                       — Interfaz pública de la HAL (neutral a la arquitectura)
├── module_abi.h                — Contrato KST y ABI de módulos (compartido con módulos)
├── modules.h / modules.c       — Registro, despacho y KST estática
├── heap.h / heap.c             — Heap del kernel (bump allocator)
├── elf.h / elf.c               — Cargador ELF64 neutral y portable
├── Makefile
├── modules/
│   └── hello/                  — Módulo de ejemplo mínimo
└── arch/
    └── x86_64/
        ├── boot.S              — Header Multiboot2, transición 32→64 bits, GDT, paginación
        ├── hal.c               — Implementación del HAL para x86_64
        ├── vga.h / vga.c       — Driver VGA en modo texto (80×25)
        ├── serial.h / serial.c — Driver serial (COM1) para consola alternativa
        ├── idt.h / idt.c       — IDT + TSS64 (IST1 para #DF)
        ├── isr.S               — 32 stubs ISR de excepciones de CPU
        ├── pmm.h / pmm.c       — Gestor de Memoria Física (bitmap allocator)
        ├── elf_arch.h          — Constantes ELF específicas de x86_64
        ├── multiboot2.h        — Estructuras de tags Multiboot2
        ├── link.ld             — Linker script del kernel
        └── modules/debug/
            └── exception_test/ — Módulo de debug (solo DEBUG=1)
```

---

## Dos Capas del Kernel

El código del kernel se divide en dos capas estrictas:

| Capa | Archivos | Regla |
|------|----------|-------|
| **Neutral** | `kernel/*.c`, `kernel/*.h` | Solo incluye `hal.h`. Portable entre arquitecturas. |
| **Arch-específica** | `kernel/arch/{arch}/` | Implementa el HAL y provee `elf_arch.h`. |

`kernel/elf.c` incluye `elf_arch.h` a través de `-I$(ARCH_DIR)` en el Makefile. Esto mantiene el cargador neutral sin hardcodear constantes de arquitectura.

---

## Build

```bash
# Desde la raíz del repositorio:
make              # kernel ELF + todos los módulos
make DEBUG=1      # incluye exception_test + -D__DEBUG__

# Desde kernel/ directamente:
make -C kernel ARCH=x86_64
```

### CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -I<arch_dir> -I. -O2
```

Flags extra para módulos: `-falign-functions=1 -falign-loops=1 -falign-jumps=1 -fPIC`

---

## Secuencia de Arranque — `arch/x86_64/boot.S`

GRUB carga el ELF del kernel en modo protegido de 32 bits y salta a `_start`.

### Configuración en 32 bits

1. **Stack**: 16 KB estático en BSS.
2. **Validación Multiboot2**: comprueba `eax == 0x36D76289`; bucle `hlt` si falla.
3. **Zero BSS**: `rep stosl` desde `bss_start` hasta `bss_end`.
4. **Tablas de páginas** (`setup_page_tables`):
   - PML4[0] → PDPT → PD con 64 × 2 MB huge pages (identity map 0–128 MB).
5. **PAE** (bit 5 de CR4), **CR3** (PML4), **Long Mode** (EFER bit 8), **Paginación** (CR0 bit 31).
6. **GDT 64-bit**: null + código (0x08) + datos (0x10).
7. **Salto lejano**: `ljmp $0x08, $long_mode_entry`.

### Entrada en 64 bits (`long_mode_entry`)

Inicializa segmentos de datos (0x10), carga argumentos en `rdi`/`rsi`, llama `kernel_main`. Al retornar: bucle `cli` + `hlt`.

---

## HAL — `hal.h` / `arch/x86_64/hal.c`

`hal.h` define la interfaz neutral. El núcleo portable solo incluye `hal.h`.

### API de Consola

| Función | Descripción |
|---------|-------------|
| `hal_console_init(type)` | Inicializa VGA o Serial según `type` |
| `hal_console_putchar(c)` | Escribe un carácter |
| `hal_console_print(str)` | Escribe una cadena |
| `hal_console_print_hex(uint64_t)` | Imprime `0xXXXXXXXXXXXXXXXX` |
| `hal_console_print_dec(uint64_t)` | Imprime número decimal |
| `hal_console_clear()` | Limpia la pantalla |
| `hal_console_getchar()` | Lee un carácter (-1 si no hay entrada) |
| `hal_console_panic_color()` | Activa color de pánico en VGA (rojo); no-op en serial |

### API de Memoria

```c
void      hal_mem_set_map(hal_mem_region_t* map, uint32_t count);
uint32_t  hal_mem_get_map(hal_mem_region_t* out, uint32_t max);
void      hal_mem_init(void);
uintptr_t hal_mem_alloc_pages(uint32_t n);   // 0 = OOM
void      hal_mem_free_pages(uintptr_t addr, uint32_t n);
```

### API de CPU e Interrupciones

```c
void hal_cpu_halt(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_enable_interrupts(void);
void hal_arch_init(uint64_t boot_magic, uint64_t boot_addr);
void hal_idt_init(void);
void hal_panic(const char* msg);

// Hook de excepción para programas ELF (ver command_kern)
void hal_set_exc_hook(void (*fn)(uint64_t vec, uint64_t rip, uint64_t err,
                                  uint64_t* out_rip, uint64_t* out_rsp));
void hal_clear_exc_hook(void);
```

---

## Driver VGA — `arch/x86_64/vga.c`

Modo texto VGA BIOS: 80 × 25 columnas en `0xB8000`. Cada celda: carácter ASCII (byte bajo) + color (byte alto). Soporte para `\n`, `\r`, `\t`, wrap de línea, scroll.

---

## Driver Serial — `arch/x86_64/serial.c`

COM1 (0x3F8) a 115200 baud, 8N1. La consola serial se activa cuando el cmdline de GRUB contiene `console=serial`. La consola activa (VGA o serial) se selecciona en `hal_console_init` y se aplica a todas las funciones `hal_console_*`.

---

## Gestor de Memoria Física — `arch/x86_64/pmm.c`

Asignador de frames basado en bitmap (1 bit/4 KB).

- `MAX_FRAMES = 131072` (512 MB / 4 KB, bitmap de 16 KB en BSS).
- `pmm_init`: marca todo usado, luego libera regiones usables de Multiboot2, luego re-marca `[0, kernel_end)` como usado.
- `pmm_alloc_pages(n)`: first-fit, devuelve dirección física base o 0.
- `pmm_free_pages(addr, n)`: con comprobación de límites (`i >= MAX_FRAMES` → stop).

---

## Heap del Kernel — `heap.c`

Bump allocator respaldado por el PMM. `HEAP_PAGES = 1024` → **4 MB**.

### `heap_sbrk(intptr_t incr)`

- `incr == 0` → devuelve el break actual (para compatibilidad con `malloc` de newlib).
- `incr > 0` → avanza el puntero bump, devuelve el puntero antiguo.
- `incr < 0` o desbordamiento → devuelve `(void*)-1`.

Conectado a `kst->mem.sbrk`, por lo que `malloc` en programas enlazados con newlib funciona automáticamente.

---

## Cargador ELF — `elf.c` / `elf.h` / `arch/x86_64/elf_arch.h`

El cargador ELF es parte de la capa neutral del kernel. Es portable: las constantes dependientes de la arquitectura provienen de `elf_arch.h`, incluido automáticamente por `-I$(ARCH_DIR)`.

### `arch/x86_64/elf_arch.h`

```c
#define ELF_CLASS    2     // ELFCLASS64
#define ELF_MACHINE  62    // EM_X86_64
#define PROG_LOAD_MIN  0x100000UL    // 1 MB — por encima del kernel
#define PROG_LOAD_MAX  0x8000000UL   // 128 MB — límite del identity map
```

### `uintptr_t elf_load(const void* data, size_t size)`

Valida: magic ELF, `ELF_CLASS`, `ET_EXEC`, `ELF_MACHINE`, `e_phoff`, `e_phentsize`.  
Por cada segmento `PT_LOAD`: comprobaciones de desbordamiento (`p_memsz > LOAD_VADDR_MAX - p_vaddr`), copia `p_filesz` bytes a `p_vaddr`, pone a cero el relleno BSS.  
Devuelve `e_entry` si al menos un segmento fue cargado, 0 en caso de error.

Expuesto en la KST como `kst->sys.elf_load`.

---

## IDT y Excepciones — `arch/x86_64/idt.c` + `isr.S`

### Estructura IDT

256 entradas `idt_entry_t` (offset 64-bit dividido, selector, atributos). Solo se instalan vectores 0–31 (excepciones de CPU), todos como `IDT_INTERRUPT_GATE` (0x8E: P=1, DPL=0, type=1110).

### TSS64 e IST1 (para #DF)

`idt_init` configura un `tss64_t` estático con una pila IST1 de 4 KB dedicada. Extiende el GDT a 5 entradas (null/code/data/TSS_low/TSS_high), recarga con `lgdt`, carga el Task Register con `ltr 0x18`. El descriptor IDT del vector 8 (#DF) tiene `ist = 1` → usa la pila IST1 incluso si la pila del kernel ha desbordado.

### Hook de Excepción

El manejador de excepciones comprueba si hay un hook de usuario registrado (vía `hal_set_exc_hook`). Si existe: rellena `*out_rip`/`*out_rsp` con la dirección de recuperación y IRETQ redirige allí. El hook se limpia automáticamente al entrar. Usado por `command_kern` para atrapar excepciones de programas ELF sin colapsar el shell.

### Manejador de Excepciones

Usa `hal_console_panic_color()` (rojo en VGA, no-op en serial) y `hal_console_*` para mostrar el nombre de la excepción, RIP, CS, RSP, RFLAGS, código de error, registros. Si hay hook: llama al hook y hace IRETQ a la dirección de recuperación. Si no hay hook: halt.

---

## Sistema de Módulos — `modules.c`

### `modules_register(start, end, cmdline)`

Llamado desde `hal_arch_init` por cada `MB2_TAG_MODULE`. Valida `MODULE_MAGIC` en `start`; si falta, el estado pasa a `FAILED`.

### `modules_run_all()`

Ejecuta todos los módulos con estado `LOADED` y header válido, excluyendo los shells de entrada (`COMMAND.KERN`, `command.com`).

### `modules_launch_entry()`

Busca `COMMAND.KERN` o `command.com` y le entrega la ejecución llamando a `entry(&kernel_kst)`.

### KST Estática — `kernel_kst`

`kst_t` estático definido en `modules.c`. Relleno en tiempo de compilación con punteros a funciones HAL e implementaciones de syscall. La dirección de `kernel_kst` nunca cambia: `kst->sys.elf_load` es tan simple como un puntero de función.

---

## ABI de Módulos — `module_abi.h`

Compartido por el kernel y todos los módulos.

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

`elf_load` se añadió en la v2. Permite a cualquier módulo cargar programas ELF sin implementar su propio loader.

### `module_header_t` (56 bytes)

```c
typedef struct {
    uint32_t magic;      // MODULE_MAGIC = 0x444F534D ("DOSM")
    char     name[32];
    char     version[16];
    uint32_t reserved;
} __attribute__((packed)) module_header_t;
```

### Macro `MODULE_HEADER(name, version)`

Incrusta el header en el offset 0 del binario plano mediante la sección `.module_header`.

---

## Linker Script del Kernel — `arch/x86_64/link.ld`

```
ENTRY(_start)
. = 1M           — dirección de carga: 1 MB físico (por encima del área BIOS)

.multiboot  ALIGN(8)   — header Multiboot2
.text       ALIGN(4K)
.rodata     ALIGN(4K)
.data       ALIGN(4K)
.bss        ALIGN(4K)  — bss_start … bss_end exportados a boot.S

kernel_end = ALIGN(4K) — exportado a heap.c y pmm.c
```
